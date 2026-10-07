#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>
#include <vector>
#include <fstream>
#include <iostream>

#include "step_parser.h"

// Load binary file
std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

uint32_t findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter,
                        vk::MemoryPropertyFlags properties) {
    vk::PhysicalDeviceMemoryProperties memProperties =
        physicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) ==
                                           properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type.");
}

int main() {

    // Create STEP file parser
    StepParser parser;
    std::shared_ptr<Shell> myShell = parser.parse("nurbs_surface.stp");

    if (myShell) {
        std::cout << "Successfully parsed Shell ID: #" << myShell->stepId << "\n";
        std::cout << "Number of Faces: " << myShell->faces.size() << "\n";
        if (!myShell->faces.empty()) {
            std::cout << "First Face bounds count: " << myShell->faces[0]->bounds.size() << "\n";
        }
    } else {
        std::cout << "Failed to parse shell geometry." << std::endl;
    }

    // Create the instance
    vk::ApplicationInfo appInfo {
        .pApplicationName = "vkNURBS application",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };
    vk::InstanceCreateInfo instanceCreateInfo {
		.flags = {}, 
		.pApplicationInfo = &appInfo
	};
    vk::Instance instance = vk::createInstance(instanceCreateInfo);

    // Find a physical device with a compute-capable queue
    vk::PhysicalDevice physicalDevice;
    uint32_t computeQueueFamily = UINT32_MAX;
    for (const vk::PhysicalDevice& device :
         instance.enumeratePhysicalDevices()) {
        auto queueFamilies = device.getQueueFamilyProperties();
        for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
            if (queueFamilies[i].queueCount > 0 &&
                (queueFamilies[i].queueFlags & vk::QueueFlagBits::eCompute)) {
                physicalDevice = device;
                computeQueueFamily = i;
                break;
            }
        }
        if (physicalDevice)
            break;
    }

	// Throw error if no compute-capable devices present
    if (!physicalDevice) {
        instance.destroy();
        throw std::runtime_error("No compute-capable Vulkan device found.");
    }

    // Create the logical device, access compute queue
    float queuePriority = 1.0f;
    vk::DeviceQueueCreateInfo queueCreateInfo{
        .flags = {},
        .queueFamilyIndex = computeQueueFamily,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority
    };
    vk::DeviceCreateInfo deviceCreateInfo{
        .flags = {},
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &queueCreateInfo
    };
    vk::Device device = physicalDevice.createDevice(deviceCreateInfo);
    vk::Queue computeQueue =
        device.getQueue(computeQueueFamily, 0);

    // Create input/output Buffer
    const uint32_t dataSize = 1024;
    const uint32_t bufferSize = dataSize * sizeof(float);
    vk::BufferCreateInfo bufferInfo {
		.flags = {}, 
		.size = bufferSize, 
		.usage = vk::BufferUsageFlagBits::eStorageBuffer
	};
    vk::Buffer buffer = device.createBuffer(bufferInfo);

    // Allocate memory
    vk::MemoryRequirements memReqs = device.getBufferMemoryRequirements(buffer);    
    uint32_t memoryTypeIndex = findMemoryType(
        physicalDevice, 
        memReqs.memoryTypeBits, 
        vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent
    );
    vk::MemoryAllocateInfo allocInfo {
        .allocationSize = memReqs.size,
        .memoryTypeIndex = memoryTypeIndex
    };
    vk::DeviceMemory bufferMemory = device.allocateMemory(allocInfo);
    device.bindBufferMemory(buffer, bufferMemory, 0);

    // Map Memory and Populate Data
    float* mappedData = (float*)device.mapMemory(bufferMemory, 0, bufferSize);
    for (uint32_t i = 0; i < dataSize; i++) {
        mappedData[i] = (float)i;
    }
    device.unmapMemory(bufferMemory);

    // Create descriptor set layout
    vk::DescriptorSetLayoutBinding binding {
		.binding = 0, 
		.descriptorType = vk::DescriptorType::eStorageBuffer, 
		.descriptorCount = 1, 
		.stageFlags = vk::ShaderStageFlagBits::eCompute
	};
    vk::DescriptorSetLayoutCreateInfo layoutInfo {
		.flags = {}, 
		.bindingCount = 1, 
		.pBindings = &binding
	};
    vk::DescriptorSetLayout descriptorSetLayout =
        device.createDescriptorSetLayout(layoutInfo);

    // Create descriptor set layout
    vk::DescriptorPoolSize poolSize { 
		.type = vk::DescriptorType::eStorageBuffer,
        .descriptorCount = 1
	};
    vk::DescriptorPoolCreateInfo poolInfo { 
		.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 
		.maxSets = 1, 
		.poolSizeCount = 1, 
		.pPoolSizes = &poolSize
	};
    vk::DescriptorPool descriptorPool = device.createDescriptorPool(poolInfo);

	// Create descriptor set
    vk::DescriptorSetAllocateInfo allocSetInfo { 
		.descriptorPool = descriptorPool, 
		.descriptorSetCount = 1, 
		.pSetLayouts = &descriptorSetLayout
	};
    vk::DescriptorSet descriptorSet =
        device.allocateDescriptorSets(allocSetInfo)[0];

    // Bind buffer to descriptor set
    vk::DescriptorBufferInfo descBufferInfo { 
		.buffer = buffer, 
		.offset = 0, 
		.range = bufferSize
	};
    vk::WriteDescriptorSet writeDesc { 
		.dstSet = descriptorSet, 
		.dstBinding = 0, 
		.dstArrayElement = 0, 
		.descriptorCount = 1, 
		.descriptorType = vk::DescriptorType::eStorageBuffer,
		.pBufferInfo = &descBufferInfo
	};
    device.updateDescriptorSets(1, &writeDesc, 0, nullptr);

    // Create the shader module
    auto shaderCode = readFile("shader.spv");
    vk::ShaderModuleCreateInfo shaderInfo {
		.flags = {}, 
		.codeSize = shaderCode.size(), 
		.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data())
	};
    vk::ShaderModule computeShaderModule =
        device.createShaderModule(shaderInfo);

	// Create the pipeline layout
    vk::PipelineShaderStageCreateInfo shaderStageInfo {
		.flags = {}, 
		.stage = vk::ShaderStageFlagBits::eCompute, 
		.module = computeShaderModule, 
		.pName = "main"
	};
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo {
		.flags = {}, 
		.setLayoutCount = 1, 
		.pSetLayouts = &descriptorSetLayout
	};
    vk::PipelineLayout pipelineLayout =
        device.createPipelineLayout(pipelineLayoutInfo);

	// Create compute pipeline
    vk::ComputePipelineCreateInfo pipelineInfo {
		.flags = {}, 
		.stage = shaderStageInfo, 
		.layout = pipelineLayout
	};
    vk::Pipeline computePipeline =
        device.createComputePipeline(nullptr, pipelineInfo).value;

    // Create command pool
    vk::CommandPoolCreateInfo cmdPoolInfo {
		.flags = {}, 
		.queueFamilyIndex = computeQueueFamily 
	};
    vk::CommandPool commandPool = device.createCommandPool(cmdPoolInfo);

	// Allocate command buffer from command pool
    vk::CommandBufferAllocateInfo cmdAllocInfo { 
		.commandPool = commandPool, 
		.level = vk::CommandBufferLevel::ePrimary, 
		.commandBufferCount = 1
	};
    vk::CommandBuffer cmdBuffer =
        device.allocateCommandBuffers(cmdAllocInfo)[0];

	// Configure command buffer
	vk::CommandBufferBeginInfo cmdBufBeginInfo { 
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit 
	};
    cmdBuffer.begin(cmdBufBeginInfo);
    cmdBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, computePipeline);
    cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
        pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    cmdBuffer.dispatch(dataSize / 256, 1, 1); 
    cmdBuffer.end();

    // Submit and wait
    vk::SubmitInfo submitInfo { 
		.waitSemaphoreCount = 0, 
		.pWaitSemaphores = nullptr, 
		.pWaitDstStageMask = nullptr, 
		.commandBufferCount = 1, 
		.pCommandBuffers = &cmdBuffer
	};
    computeQueue.submit(submitInfo);
    computeQueue.waitIdle();

    // Read results back
    mappedData = (float*)device.mapMemory(bufferMemory, 0, bufferSize);
    std::cout << "Result [5]: " << mappedData[5] << " (Expected 25)\n";
    device.unmapMemory(bufferMemory);

    // Cleanup
    device.destroyCommandPool(commandPool);
    device.destroyPipeline(computePipeline);
    device.destroyPipelineLayout(pipelineLayout);
    device.destroyShaderModule(computeShaderModule);
    device.destroyDescriptorPool(descriptorPool);
    device.destroyDescriptorSetLayout(descriptorSetLayout);
    device.destroyBuffer(buffer);
    device.freeMemory(bufferMemory);
    device.destroy();
    instance.destroy();
    return 0;
}