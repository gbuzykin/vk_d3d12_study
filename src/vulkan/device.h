#pragma once

#include "buffer.h"
#include "command_buffer.h"
#include "dev_queue.h"

#include <uxs/dynarray.h>

namespace app3d::rel::vulkan {

class RenderingDriver;
class PhysicalDevice;

class Device final : public util::ref_counter, public IDevice {
 public:
    Device(RenderingDriver& instance, PhysicalDevice& physical_device);
    ~Device() override;

    bool create(const uxs::db::value& caps);
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

    bool updateBuffer(const void* data, VkDeviceSize data_size, VkBuffer dst, VkDeviceSize offset,
                      VkPipelineStageFlags generating_stages, VkPipelineStageFlags consuming_stages,
                      VkAccessFlags current_access, VkAccessFlags new_access,
                      std::span<const VkSemaphore> signal_semaphores);
    bool updateImage(const void* data, VkDeviceSize data_size, VkImage dst, VkImageSubresourceLayers subresource,
                     VkOffset3D offset, VkExtent3D extent, VkPipelineStageFlags generating_stages,
                     VkPipelineStageFlags consuming_stages, VkAccessFlags current_access, VkAccessFlags new_access,
                     VkImageLayout current_layout, VkImageLayout new_layout, VkImageAspectFlags aspect,
                     std::span<const VkSemaphore> signal_semaphores);

    VkDevice operator~() { return device_; }
    PhysicalDevice& getPhysicalDevice() { return physical_device_; }
    VmaAllocator getAllocator() { return allocator_; }
    DevQueue& getGraphicsQueue() { return graphics_queue_; }
    DevQueue& getComputeQueue() { return compute_queue_; }

    //@{ IDevice
    util::ref_counter& getRefCounter() override { return *this; }
    bool waitDevice() override;
    util::ref_ptr<IShaderModule> createShaderModule(std::span<const std::uint32_t> source) override;
    util::ref_ptr<IPipelineLayout> createPipelineLayout(const uxs::db::value& config) override;
    util::ref_ptr<IPipeline> createPipeline(IRenderTarget& render_target, IPipelineLayout& pipeline_layout,
                                            std::span<IShaderModule* const> shader_modules,
                                            const uxs::db::value& config) override;
    util::ref_ptr<IBuffer> createBuffer(std::size_t size, BufferType type) override;
    util::ref_ptr<ITexture> createTexture(const TextureOpts& opts) override;
    util::ref_ptr<ISampler> createSampler(const SamplerOpts& opts) override;
    util::ref_ptr<IDescriptorSet> createDescriptorSet(IPipelineLayout& pipeline_layout) override;
    //@}

 private:
    util::ref_ptr<RenderingDriver> instance_;
    PhysicalDevice& physical_device_;
    VkDevice device_{VK_NULL_HANDLE};
    DevQueue graphics_queue_;
    DevQueue compute_queue_;
    DevQueue transfer_queue_;

    VmaAllocator allocator_{VK_NULL_HANDLE};
    VkDescriptorPool descriptor_pool_{VK_NULL_HANDLE};

    struct StagingBuffer {
        VkBuffer handle{VK_NULL_HANDLE};
        VmaAllocation allocation{VK_NULL_HANDLE};
        std::uint32_t size = 0;
    };

    struct TransferKit {
        VkFence fence{VK_NULL_HANDLE};
        StagingBuffer staging_buffer;
        CommandBuffer command_buffer;
    };

    static constexpr std::uint32_t TRANSFER_KIT_COUNT = 1;
    static constexpr std::uint64_t FINISH_TRANSFER_TIMEOUT = 500'000'000;
    std::uint32_t current_transfer_kit_ = 0;
    uxs::inline_dynarray<TransferKit, TRANSFER_KIT_COUNT> transfer_kits_;

    bool createStagingBuffer(VkDeviceSize size, StagingBuffer& buffer);

    bool createDescriptorPool(std::uint32_t max_sets, std::span<const VkDescriptorPoolSize> descriptor_types,
                              VkDescriptorPool& descriptor_pool);
};

}  // namespace app3d::rel::vulkan
