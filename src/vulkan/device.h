#pragma once

#include "buffer.h"
#include "command_buffer.h"
#include "dev_queue.h"

#include <uxs/dynarray.h>

namespace app3d::rel::vulkan {

class RenderingDriver;
class PhysicalDevice;
class ShaderModule;
class Pipeline;
class Buffer;
class Texture;
class Sampler;
class DescriptorSet;

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

    bool updateBuffer(const void* data, VkDeviceSize data_size, VkBuffer dst, VkDeviceSize dst_offset,
                      VkAccessFlags dst_current_access, VkAccessFlags dst_new_access,
                      VkPipelineStageFlags dst_generating_stages, VkPipelineStageFlags dst_consuming_stages,
                      std::span<const VkSemaphore> signal_semaphores);
    bool updateImage(const void* data, VkDeviceSize data_size, VkImage dst, VkImageSubresourceLayers dst_subresource,
                     VkOffset3D dst_offset, VkExtent3D dst_extent, VkImageLayout dst_current_layout,
                     VkImageLayout dst_new_layout, VkAccessFlags dst_current_access, VkAccessFlags dst_new_access,
                     VkImageAspectFlags dst_aspect, VkPipelineStageFlags dst_generating_stages,
                     VkPipelineStageFlags dst_consuming_stages, std::span<const VkSemaphore> signal_semaphores);

    VkDevice operator~() { return device_; }
    PhysicalDevice& getPhysicalDevice() { return physical_device_; }
    VmaAllocator getAllocator() { return vma_allocator_; }
    DevQueue& getGraphicsQueue() { return graphics_queue_; }
    DevQueue& getPresentQueue() { return present_queue_; }
    DevQueue& getComputeQueue() { return compute_queue_; }

    //@{ IDevice
    IShaderModule* createShaderModule(std::span<const std::uint32_t> source) override;
    IPipeline* createPipeline(IRenderTarget& render_target, std::span<IShaderModule* const> shader_modules,
                              const uxs::db::value& config) override;
    IBuffer* createBuffer(std::size_t size, BufferType type) override;
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

    VmaAllocator vma_allocator_{VK_NULL_HANDLE};
    VkDescriptorPool descriptor_pool_{VK_NULL_HANDLE};

    std::vector<std::unique_ptr<ShaderModule>> shader_modules_;
    std::vector<std::unique_ptr<Pipeline>> pipelines_;
    std::vector<std::unique_ptr<Buffer>> buffers_;
    std::vector<std::unique_ptr<Texture>> textures_;
    std::vector<std::unique_ptr<Sampler>> samplers_;
    std::vector<std::unique_ptr<DescriptorSet>> descriptor_sets_;

    struct TransferKit {
        explicit TransferKit(Device& device) : staging_buffer(device) {}
        VkFence fence;
        Buffer staging_buffer;
        CommandBuffer command_buffer;
    };

    static constexpr unsigned TRANSFER_KIT_COUNT = 3;
    unsigned current_transfer_kit_ = 0;
    uxs::inline_dynarray<TransferKit, TRANSFER_KIT_COUNT> transfer_kits_;

    bool createDescriptorPool(bool free_individual_sets, std::uint32_t max_sets_count,
                              std::span<const VkDescriptorPoolSize> descriptor_types,
                              VkDescriptorPool& descriptor_pool);
};

}  // namespace app3d::rel::vulkan
