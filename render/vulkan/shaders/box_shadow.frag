#version 450
#extension GL_ARB_shading_language_include : require

// Writeup: https://madebyevan.com/shaders/fast-rounded-rectangle-shadows/

// Falloff exponent applied to the gaussian mask, in the spirit of Hyprland's
// shadow_render_power. 1.0 is the plain gaussian; higher keeps opacity near
// the window and thins the tail out faster.
#define SHADOW_FALLOFF_POWER 2.0

layout(location = 0) out vec4 out_color;
layout(push_constant) uniform UBO {
	layout(offset = 80) vec4 color;
	vec2 size;
	vec2 position;
	float blur_sigma;
	float corner_radius;

	vec2 clip_size;
	vec2 clip_position;
	float clip_radius_top_left;
	float clip_radius_top_right;
	float clip_radius_bottom_left;
	float clip_radius_bottom_right;
} data;

layout (constant_id = 0) const int EFFECTS = 0;

// Matches enum fx_quad_shader_effects
#define EFFECT_CLIPPING 2

#include "corner_alpha.glsl"

float gaussian(float x, float sigma) {
	const float pi = 3.141592653589793;
	return exp(-(x * x) / (2.0 * sigma * sigma)) / (sqrt(2.0 * pi) * sigma);
}

// approximates the error function, needed for the gaussian integral
vec2 erf(vec2 x) {
	vec2 s = sign(x), a = abs(x);
	x = 1.0 + (0.278393 + (0.230389 + 0.078108 * (a * a)) * a) * a;
	x *= x;
	return s - s / (x * x);
}

// the blurred mask along the x dimension
float rounded_box_shadow_x(float x, float y, float sigma, float corner, vec2 half_size) {
	float delta = min(half_size.y - corner - abs(y), 0.0);
	float curved = half_size.x - corner + sqrt(max(0.0, corner * corner - delta * delta));
	vec2 integral = 0.5 + 0.5 * erf((x + vec2(-curved, curved)) * (sqrt(0.5) / sigma));
	return integral.y - integral.x;
}

float rounded_box_shadow(vec2 lower, vec2 upper, vec2 point, float sigma, float corner_radius) {
	// Center everything to make the math easier
	vec2 center = (lower + upper) * 0.5;
	vec2 half_size = (upper - lower) * 0.5;
	point -= center;

	// The signal is only non-zero in a limited range, so don't waste samples
	float low = point.y - half_size.y;
	float high = point.y + half_size.y;
	float start = clamp(-3.0 * sigma, low, high);
	float end = clamp(3.0 * sigma, low, high);

	// Accumulate samples (we can get away with surprisingly few samples)
	float step_size = (end - start) / 4.0;
	float y = start + step_size * 0.5;
	float value = 0.0;
	for (int i = 0; i < 4; i++) {
		value += rounded_box_shadow_x(point.x, point.y - y, sigma, corner_radius, half_size)
			* gaussian(y, sigma) * step_size;
		y += step_size;
	}

	return value;
}

void main() {
	// The gaussian runs at blur_sigma * 0.5 and its tail reaches 3 sigma, so
	// the node box is inset by 1.5 * blur_sigma to get the casting rect.
	float alpha = data.color.a * rounded_box_shadow(
		data.position + data.blur_sigma * 1.5,
		data.position + data.size - data.blur_sigma * 1.5,
		gl_FragCoord.xy,
		data.blur_sigma * 0.5,
		data.corner_radius
	);
	alpha = pow(alpha, SHADOW_FALLOFF_POWER);

	// Clipping: punch the window's own rect out of the shadow
	if ((EFFECTS & EFFECT_CLIPPING) == EFFECT_CLIPPING) {
		alpha *= corner_alpha(
			data.clip_size - 1.5,
			data.clip_position + 0.75,
			true,
			data.clip_radius_top_left,
			data.clip_radius_top_right,
			data.clip_radius_bottom_left,
			data.clip_radius_bottom_right
		);
	}

	// The Vulkan pipeline blends premultiplied; the GLES2 path gets the same
	// result from a straight-alpha blend func.
	out_color = vec4(data.color.rgb * alpha, alpha);
}
