#ifndef VR_LAYER_H
#define VR_LAYER_H

#include <cstdint>
#include <memory>

#include <components/stereo/stereomanager.hpp>

namespace VR
{
    class Swapchain;
    class Space;

    // Describes a subregion of a swapchain
    struct SubImage
    {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t index = 0;
    };

    struct Layer
    {
    public:
        virtual ~Layer() {};
    };

    struct ProjectionLayerView
    {
    public:
        ProjectionLayerView();
        ~ProjectionLayerView();

        std::shared_ptr<Swapchain> colorSwapchain;
        std::shared_ptr<Swapchain> depthSwapchain;
        SubImage subImage;
        Stereo::View view;
    };

    struct ProjectionLayer : public Layer
    {
    public:
        ProjectionLayer();
        ~ProjectionLayer() override;

        std::array<ProjectionLayerView, 2> views;
    };
}

#endif
