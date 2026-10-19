// Copyright (c) 2013-2015 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: http://opensource.org/licenses/AFL-3.0

// VectorPie port: SSVStart's Camera without the window. Same order of
// operations: turn/pan/zoom are applied on the next update(mFT), scaled by
// the frame time; apply() returns the view with the skew multiplied into
// its size.

#pragma once

#include <SFML/Graphics/View.hpp>
#include <SFML/System/Vector2.hpp>

#include <SSVUtils/Core/Common/Frametime.hpp>
#include <SSVUtils/Core/Utils/Math.hpp>

#include <cmath>

namespace ssvs {

class Camera
{
private:
    sf::View view, computedView;
    sf::Vector2f nextPan, skew{1.f, 1.f};
    float nextZoomFactor{1.f}, nextRotation{0.f};
    bool invalid{false}, mustRecompute{true};

public:
    explicit Camera(const sf::View& mView) : view{mView}
    {}

    [[nodiscard]] sf::View apply()
    {
        if(mustRecompute)
        {
            computedView = view;
            computedView.setSize({computedView.getSize().x * skew.x,
                computedView.getSize().y * skew.y});
            mustRecompute = false;
        }

        return computedView;
    }

    void pan(const sf::Vector2f& mVec)
    {
        nextPan += mVec;
        invalid = true;
    }

    void zoomIn(float mFactor)
    {
        nextZoomFactor /= mFactor;
        invalid = true;
    }

    void zoomOut(float mFactor)
    {
        nextZoomFactor *= mFactor;
        invalid = true;
    }

    void turn(float mDeg)
    {
        nextRotation += mDeg;
        invalid = true;
    }

    void update(ssvu::FT mFT)
    {
        if(!invalid) return;

        view.setCenter(view.getCenter() + nextPan);
        view.zoom(std::pow(nextZoomFactor, mFT));
        view.rotate(nextRotation * mFT);

        mustRecompute = true;
        nextPan = {0.f, 0.f};
        nextZoomFactor = 1.f;
        nextRotation = 0.f;
        invalid = false;
    }

    void setView(const sf::View& mView) noexcept
    {
        view = mView;
        mustRecompute = true;
    }

    void setRotation(float mDeg) noexcept
    {
        view.setRotation(mDeg);
        mustRecompute = true;
    }

    void setSkew(const sf::Vector2f& mSkew) noexcept
    {
        skew = mSkew;
        mustRecompute = true;
    }

    void setCenter(const sf::Vector2f& mPosition) noexcept
    {
        view.setCenter(mPosition);
        mustRecompute = true;
    }

    [[nodiscard]] const sf::View& getView() const noexcept
    {
        return view;
    }

    [[nodiscard]] float getRotation() const noexcept
    {
        return view.getRotationDeg();
    }

    [[nodiscard]] const sf::Vector2f& getSkew() const noexcept
    {
        return skew;
    }

    [[nodiscard]] const sf::Vector2f& getCenter() const noexcept
    {
        return view.getCenter();
    }
};

} // namespace ssvs
