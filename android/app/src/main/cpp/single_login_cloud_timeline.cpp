#include "single_login_cloud_timeline.h"

#include <algorithm>
#include <cmath>

namespace nevergone::single_login_cloud_timeline {
namespace {

constexpr float kAlpha76 = 76.0f / 255.0f;
constexpr float kAlpha102 = 102.0f / 255.0f;

float lerp(float a, float b, double t) {
    const double clamped = std::clamp(t, 0.0, 1.0);
    return static_cast<float>(static_cast<double>(a) +
        (static_cast<double>(b) - static_cast<double>(a)) * clamped);
}

double cycle_time(double seconds, double duration) {
    if (seconds <= 0.0 || duration <= 0.0) return 0.0;
    const double value = std::fmod(seconds, duration);
    return value >= 0.0 ? value : value + duration;
}

CloudPose linear_cycle(
    double seconds,
    double duration,
    float start_x,
    float start_y,
    float end_x,
    float end_y,
    float alpha,
    float rotation,
    int z,
    int frame_index) {
    const double local = cycle_time(seconds, duration);
    const double t = duration > 0.0 ? local / duration : 0.0;
    CloudPose pose;
    pose.x = lerp(start_x, end_x, t);
    pose.y = lerp(start_y, end_y, t);
    pose.alpha = alpha;
    pose.rotation_degrees = rotation;
    pose.z = z;
    pose.frame_index = frame_index;
    return pose;
}

CloudPose faded_three_segment_cycle(
    double seconds,
    double fade_seconds,
    double middle_seconds,
    float reset_x,
    float reset_y,
    float first_x,
    float middle_x,
    float end_x,
    float y,
    int z,
    int frame_index) {
    const double duration = fade_seconds + middle_seconds + fade_seconds;
    const double local = cycle_time(seconds, duration);

    CloudPose pose;
    pose.y = y;
    pose.rotation_degrees = 0.0f;
    pose.z = z;
    pose.frame_index = frame_index;

    if (local < fade_seconds) {
        const double t = local / fade_seconds;
        pose.x = lerp(reset_x, first_x, t);
        pose.alpha = lerp(0.0f, kAlpha102, t);
        return pose;
    }

    const double after_fade_in = local - fade_seconds;
    if (after_fade_in < middle_seconds) {
        const double t = after_fade_in / middle_seconds;
        pose.x = lerp(first_x, middle_x, t);
        pose.alpha = kAlpha102;
        return pose;
    }

    const double after_middle = after_fade_in - middle_seconds;
    const double t = after_middle / fade_seconds;
    pose.x = lerp(middle_x, end_x, t);
    pose.alpha = lerp(kAlpha102, 0.0f, t);
    return pose;
}

}  // namespace

Sample sample(
    double scene_seconds,
    float visible_width,
    float visible_height,
    float primary_width,
    float primary_height) {
    Sample result;
    if (scene_seconds < 0.0 || visible_width <= 0.0f || visible_height <= 0.0f ||
        primary_width <= 0.0f || primary_height <= 0.0f) {
        return result;
    }

    // InitUI iteration 0. The original client gives these three nodes fixed
    // rotation/opacity and repeats an instantaneous reset + one MoveTo.
    result.clouds[0] = linear_cycle(
        scene_seconds,
        16.0,
        -0.5f * primary_width,
        20.0f - 0.5f * visible_height,
        0.7f * visible_width,
        -primary_height,
        kAlpha76,
        30.0f,
        3,
        0);
    result.clouds[1] = linear_cycle(
        scene_seconds,
        18.0,
        -0.5f * primary_width,
        100.0f + 0.5f * visible_height,
        0.7f * visible_width,
        120.0f - primary_height,
        kAlpha102,
        30.0f,
        2,
        1);
    result.clouds[2] = linear_cycle(
        scene_seconds,
        20.0,
        -0.5f * primary_width,
        290.0f + 0.5f * visible_height,
        0.7f * visible_width,
        330.0f - primary_height,
        kAlpha76,
        40.0f,
        1,
        1);

    // InitUI iteration 1. Each node starts transparent at x=352 and travels
    // across three Spawn(MoveTo, FadeTo) / MoveTo phases before repeating.
    const float delta = 352.0f - 0.8f * visible_width;
    result.clouds[3] = faded_three_segment_cycle(
        scene_seconds,
        2.0,
        12.0,
        352.0f,
        -35.0f,
        352.0f + 0.2f * delta,
        352.0f + 0.8f * delta,
        0.8f * visible_width,
        -35.0f,
        3,
        0);
    result.clouds[4] = faded_three_segment_cycle(
        scene_seconds,
        2.0,
        14.0,
        352.0f,
        65.0f,
        352.0f + 0.1f * delta,
        352.0f + 0.9f * delta,
        0.8f * visible_width,
        65.0f,
        2,
        1);
    result.clouds[5] = faded_three_segment_cycle(
        scene_seconds,
        2.5,
        15.0,
        352.0f,
        195.0f,
        352.0f + 0.1f * delta,
        352.0f + 0.9f * delta,
        0.8f * visible_width,
        195.0f,
        1,
        1);

    // InitUI iteration 2. Again the sequence is an instantaneous reset plus
    // a single repeating MoveTo, using the primary cloud's content size in
    // all three recovered coordinate calculations.
    result.clouds[6] = linear_cycle(
        scene_seconds,
        16.0,
        -15.0f,
        -0.5f * primary_height,
        visible_width + primary_width,
        375.0f,
        kAlpha76,
        -15.0f,
        3,
        0);
    result.clouds[7] = linear_cycle(
        scene_seconds,
        18.0,
        568.0f,
        100.0f - 0.5f * primary_height,
        visible_width + primary_width,
        475.0f,
        kAlpha102,
        -15.0f,
        2,
        1);
    result.clouds[8] = linear_cycle(
        scene_seconds,
        20.0,
        -25.0f,
        250.0f - 0.5f * primary_height,
        visible_width + primary_width,
        625.0f,
        kAlpha76,
        -25.0f,
        1,
        1);

    return result;
}

}  // namespace nevergone::single_login_cloud_timeline
