#pragma once

#include "command_buffer.h"
#include "dev_queue.h"

namespace app3d::rel::vulkan {

class RenderingDriver;
class PhysicalDevice;
class ShaderModule;
class Pipeline;
class Buffer;
class Texture;
class Sampler;
class DescriptorSet;

class MappedMemory {
 public:
    MappedMemory(VkDevice device, VkDeviceMemory memory_object) : device_(device), memory_object_(memory_object) {}
    ~MappedMemory() { release(); }
    MappedMemory(const MappedMemory&) = delete;
    MappedMemory& operator=(const MappedMemory&) = delete;

    void* ptr() const { return ptr_; }
    bool map(VkDeviceSize offset, VkDeviceSize data_size);
    void release() {
        if (!ptr_) { return; }
        vkUnmapMemory(device_, memory_object_);
        ptr_ = nullptr;
    }

    VkDeviceMemory operator~() { return memory_object_; }

 private:
    VkDevice device_;
    VkDeviceMemory memory_object_;
    void* ptr_ = nullptr;
};

class Device final : public IDevice {
 public:
    Device(RenderingDriver& instance, PhysicalDevice& physical_device);
    ~Device() override;
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    bool create(const uxs::db::value& caps);
    void finalize();
    bool waitDevice();
    bool createSemaphore(VkSemaphore& semaphore);
    bool createFence(bool signaled, VkFence& fence);
    bool waitForFences(std::span<const VkFence> fences, VkBool32 wait_for_all, std::uint64_t timeout);
    bool resetFences(std::span<const VkFence> fences);

    bool obtainDescriptorSet(VkDescriptorSetLayout, VkDescriptorSet& descriptor_set);
    void releaseDescriptorSet(VkDescriptorSet descriptor_set);
    void updateDescriptorSets(std::span<const VkWriteDescriptorSet> write_descriptors,
                              std::span<const VkCopyDescriptorSet> copy_descriptors) {
        vkUpdateDescriptorSets(device_, std::uint32_t(write_descriptors.size()), write_descriptors.data(),
                               std::uint32_t(copy_descriptors.size()), copy_descriptors.data());
    }

    bool writeBufferInDeviceLocalMemory(VkDeviceSize data_size, const void* data, VkBuffer dst, VkDeviceSize dst_offset,
                                        VkAccessFlags dst_current_access, VkAccessFlags dst_new_access,
                                        VkPipelineStageFlags dst_generating_stages,
                                        VkPipelineStageFlags dst_consuming_stages,
                                        std::span<const VkSemaphore> signal_semaphores);
    bool writeImageInDeviceLocalMemory(VkDeviceSize data_size, const void* data, VkImage dst,
                                       VkImageSubresourceLayers dst_subresource, VkOffset3D dst_offset,
                                       VkExtent3D dst_extent, VkImageLayout dst_current_layout,
                                       VkImageLayout dst_new_layout, VkAccessFlags dst_current_access,
                                       VkAccessFlags dst_new_access, VkImageAspectFlags dst_aspect,
                                       VkPipelineStageFlags dst_generating_stages,
                                       VkPipelineStageFlags dst_consuming_stages,
                                       std::span<const VkSemaphore> signal_semaphores);

    VkDevice operator~() { return device_; }
    PhysicalDevice& getPhysicalDevice() { return physical_device_; }
    DevQueue& getGraphicsQueue() { return graphics_queue_; }
    DevQueue& getPresentQueue() { return present_queue_; }
    DevQueue& getComputeQueue() { return compute_queue_; }

    //@{ IDevice
    IShaderModule* createShaderModule(std::span<const std::uint32_t> source) override;
    IPipeline* createPipeline(IRenderTarget& render_target, std::span<IShaderModule* const> shader_modules,
                              const uxs::db::value& config) override;
    IBuffer* createBuffer(std::size_t size) override;
    ITexture* createTexture(Extent3u extent) override;
    ISampler* createSampler() override;
    IDescriptorSet* createDescriptorSet(IPipeline& pipeline) override;
    //@}

 private:
    RenderingDriver& instance_;
    PhysicalDevice& physical_device_;
    VkDevice device_{VK_NULL_HANDLE};
    DevQueue graphics_queue_;
    DevQueue compute_queue_;
    DevQueue transfer_queue_;
    DevQueue present_queue_;

    VkDescriptorPool descriptor_pool_{VK_NULL_HANDLE};

    std::vector<std::unique_ptr<ShaderModule>> shader_modules_;
    std::vector<std::unique_ptr<Pipeline>> pipelines_;
    std::vector<std::unique_ptr<Buffer>> buffers_;
    std::vector<std::unique_ptr<Texture>> textures_;
    std::vector<std::unique_ptr<Sampler>> samplers_;
    std::vector<std::unique_ptr<DescriptorSet>> descriptor_sets_;

    CommandBuffer transfer_command_buffer_;

    static constexpr std::uint64_t FINISH_TRANSFER_TIMEOUT = 500'000'000;

    bool createDescriptorPool(std::uint32_t max_sets_count, std::span<const VkDescriptorPoolSize> descriptor_types,
                              VkDescriptorPool& descriptor_pool);

    bool writeToHostVisibleMemory(VkDeviceMemory memory_object, VkDeviceSize offset, VkDeviceSize data_size,
                                  const void* data);
};

}  // namespace app3d::rel::vulkan
