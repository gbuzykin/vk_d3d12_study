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

#define DEVICE_LEVEL_VK_FUNCTION(name) \
    template<typename... Args> \
    auto name(Args&&... args) { \
        return vk_funcs_.name(device_, std::forward<Args>(args)...); \
    }

#define DEVICE_LEVEL_VK_FUNCTION_FROM_EXTENSION(name, extension) \
    template<typename... Args> \
    auto name(Args&&... args) { \
        return vk_funcs_.name(device_, std::forward<Args>(args)...); \
    }

#include "vulkan_function_list.inl"

    const DeviceVkFuncTable& getVkFuncs() { return vk_funcs_; }

    bool create(const uxs::db::value& caps);
    bool createSemaphore(VkSemaphore& semaphore);
    bool createFence(bool signaled, VkFence& fence);
    bool waitForFences(std::span<const VkFence> fences, VkBool32 wait_for_all, std::uint64_t timeout);
    bool resetFences(std::span<const VkFence> fences);

    bool updateBuffer(std::span<const std::uint8_t> data, VkBuffer dst, VkDeviceSize offset,
                      VkPipelineStageFlags generating_stages, VkPipelineStageFlags consuming_stages,
                      VkAccessFlags current_access, VkAccessFlags new_access,
                      std::span<const VkSemaphore> signal_semaphores);
    bool updateImage(const std::uint8_t* data, VkImage dst, Format format, std::uint32_t first_subresource,
                     std::span<const UpdateTextureDesc> update_subresource_descs,
                     VkPipelineStageFlags generating_stages, VkPipelineStageFlags consuming_stages,
                     VkAccessFlags current_access, VkAccessFlags new_access, VkImageLayout current_layout,
                     VkImageLayout new_layout, VkImageAspectFlags aspect,
                     std::span<const VkSemaphore> signal_semaphores);

    void updateDescriptorSets(std::span<const VkWriteDescriptorSet> write_descriptors,
                              std::span<const VkCopyDescriptorSet> copy_descriptors) {
        vkUpdateDescriptorSets(std::uint32_t(write_descriptors.size()), write_descriptors.data(),
                               std::uint32_t(copy_descriptors.size()), copy_descriptors.data());
    }

    PhysicalDevice& getPhysicalDevice() { return physical_device_; }
    VmaAllocator getAllocator() { return allocator_; }
    DevQueue& getGraphicsQueue() { return graphics_queue_; }
    DevQueue& getComputeQueue() { return compute_queue_; }

    //@{ IDevice
    util::ref_counter& getRefCounter() override { return *this; }
    bool waitDevice() override;
    util::ref_ptr<ISwapChain> createSwapChain(ISurface& surface, const uxs::db::value& opts) override;
    util::ref_ptr<IShaderModule> createShaderModule(DataBlob bytecode) override;
    util::ref_ptr<IPipelineLayout> createPipelineLayout(const uxs::db::value& config) override;
    util::ref_ptr<IPipeline> createPipeline(IRenderTarget& render_target, IPipelineLayout& pipeline_layout,
                                            std::span<IShaderModule* const> shader_modules,
                                            const uxs::db::value& config) override;
    util::ref_ptr<IBuffer> createBuffer(BufferType type, std::uint64_t size) override;
    util::ref_ptr<ITexture> createTexture(const TextureDesc& desc) override;
    util::ref_ptr<ISampler> createSampler(const SamplerDesc& desc) override;
    //@}

 private:
    util::ref_ptr<RenderingDriver> instance_;
    PhysicalDevice& physical_device_;
    DeviceVkFuncTable vk_funcs_;
    VkDevice device_{VK_NULL_HANDLE};
    VmaAllocator allocator_{VK_NULL_HANDLE};
    DevQueue graphics_queue_;
    DevQueue compute_queue_;
    DevQueue transfer_queue_;

    struct StagingBuffer {
        VkBuffer handle{VK_NULL_HANDLE};
        VmaAllocation allocation{VK_NULL_HANDLE};
        VkDeviceSize size = 0;
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
};

}  // namespace app3d::rel::vulkan
