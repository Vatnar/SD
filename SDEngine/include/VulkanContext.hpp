#pragma once

#include "VulkanConfig.hpp"
#include "GlfwContext.hpp"
#include "Window.hpp"


class VulkanContext
{
public:
    VulkanContext(const GlfwContext& glfwCtx, const Window& window);
    ~VulkanContext();

    vk::UniqueSwapchainKHR& CreateSwapchain();

    vk::UniqueInstance&                 GetInstance() { return mInstance; }
    std::vector<const char *>&          GetDeviceExtensions() { return mDeviceExts; }
    vk::PhysicalDevice&                 GetPhysicalDevice() { return mPhysDev; }
    vk::UniqueDevice&                   GetVulkanDevice() { return mVulkanDevice; }
    vk::PhysicalDeviceFeatures2&        GetFeatures2() { return mFeatures2; }
    vk::PhysicalDeviceVulkan12Features& GetFeatures12() { return mFeatures12; }
    vk::PhysicalDeviceVulkan13Features& GetFeatures13() { return mFeatures13; }
    [[nodiscard]] uint32_t              GetGraphicsFamilyIndex() const { return mGraphicsFamilyIndex; }
    vk::UniqueSwapchainKHR&             GetSwapchain() { return mSwapchain; }
    vk::SurfaceFormatKHR&               GetSurfaceFormat() { return mSurfaceFormat; }
    vk::Extent2D&                       GetSwapchainExtent() { return mSwapchainExtent; }
    vk::SwapchainCreateInfoKHR&         GetSwapchainCreateInfo() { return mSwapchainCreateInfo; }

private:
    void               SetupQueues();
    vk::UniqueInstance CreateVulkanApplicationInstance();
    void               SetupDeviceExtensions();
    void               CreateVulkanDevice();

private:
    vk::UniqueInstance                 mInstance;
    std::vector<const char *>          mDeviceExts;
    vk::PhysicalDevice                 mPhysDev;
    vk::PhysicalDeviceFeatures2        mFeatures2;
    vk::PhysicalDeviceVulkan12Features mFeatures12;
    vk::PhysicalDeviceVulkan13Features mFeatures13;
    const GlfwContext&                 mGlfwCtx;
    const Window&                      mWindow;
    uint32_t                           mGraphicsFamilyIndex{};
    vk::UniqueSurfaceKHR               mSurface;
    vk::UniqueDevice                   mVulkanDevice;
    vk::UniqueSwapchainKHR             mSwapchain;
    vk::SurfaceFormatKHR               mSurfaceFormat;
    vk::Extent2D                       mSwapchainExtent;
    vk::SurfaceCapabilitiesKHR         mSurfaceCapabilities;
    vk::SwapchainCreateInfoKHR         mSwapchainCreateInfo;
};
