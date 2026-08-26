#include <math.h>
#include <stdlib.h>

#include "scenefx/types/fx/clipped_region.h"
#include "types/fx/clipped_region.h"

struct fx_corner_radii fx_corner_radii_extend(struct fx_corner_radii corners, int extend) {
	return (struct fx_corner_radii) {
		.top_left =  corners.top_left == 0 ? 0 : (corners.top_left + extend),
		.top_right = corners.top_right == 0 ? 0 : (corners.top_right + extend),
		.bottom_left = corners.bottom_left == 0 ? 0 : (corners.bottom_left + extend),
		.bottom_right = corners.bottom_right == 0 ? 0 : (corners.bottom_right + extend),
	};
}

void fx_corner_radii_transform(enum wl_output_transform transform,
		struct fx_corner_radii *corners) {
	if (transform & WL_OUTPUT_TRANSFORM_FLIPPED) {
		*corners = (struct fx_corner_radii){
			corners->top_right,
			corners->top_left,
			corners->bottom_left,
			corners->bottom_right
		};
	}

	unsigned int turns = transform & (WL_OUTPUT_TRANSFORM_90 | WL_OUTPUT_TRANSFORM_180 | WL_OUTPUT_TRANSFORM_270);
	if (turns > 0) {
		uint16_t points[4] = {corners->top_left, corners->top_right, corners->bottom_right, corners->bottom_left};
		*corners = (struct fx_corner_radii){
			points[turns % 4],
			points[(turns + 1) % 4],
			points[(turns + 2) % 4],
			points[(turns + 3) % 4],
		};
	}
}

struct fx_corner_fradii fx_corner_radii_scale(struct fx_corner_radii corners, float scale) {
	return (struct fx_corner_fradii) {
	(float)corners.top_left * scale,
	(float)corners.top_right * scale,
	(float)corners.bottom_right * scale,
	(float)corners.bottom_left * scale,
	};
}

bool fx_corner_radii_eq(const struct fx_corner_radii lhs, const struct fx_corner_radii rhs) {
	return lhs.top_left == rhs.top_left
		&& lhs.top_right == rhs.top_right
		&& lhs.bottom_right == rhs.bottom_right
		&& lhs.bottom_left == rhs.bottom_left;
}

bool fx_corner_radii_is_empty(const struct fx_corner_radii* corners) {
	return corners->top_left == 0
		&& corners->top_right == 0
		&& corners->bottom_right == 0
		&& corners->bottom_left == 0;
}

bool fx_corner_fradii_is_empty(const struct fx_corner_fradii* corners) {
	return corners->top_left == 0.0
		&& corners->top_right == 0.0
		&& corners->bottom_right == 0.0
		&& corners->bottom_left == 0.0;
}

static int arc_inset(float radius, int row) {
	if (row >= (int)radius) {
		return 0;
	}
	float dy = radius - row - 0.5f;
	return lroundf(radius - sqrtf(radius * radius - dy * dy));
}

void clipped_fregion_to_region(const struct clipped_fregion *fregion,
		pixman_region32_t *region) {
	const struct wlr_box *area = &fregion->area;
	if (wlr_box_empty(area)) {
		pixman_region32_init(region);
		return;
	}

	float max_radius = fminf(area->width, area->height) / 2.0f;
	float top_left = fminf(fregion->corners.top_left, max_radius);
	float top_right = fminf(fregion->corners.top_right, max_radius);
	float bottom_right = fminf(fregion->corners.bottom_right, max_radius);
	float bottom_left = fminf(fregion->corners.bottom_left, max_radius);

	int top = fmaxf(top_left, top_right);
	int bottom = fmaxf(bottom_left, bottom_right);
	pixman_box32_t *boxes = malloc((top + bottom + 1) * sizeof(*boxes));
	if (boxes == NULL) {
		pixman_region32_init_rect(region, area->x, area->y, area->width, area->height);
		return;
	}

	int len = 0;
	for (int row = 0; row < top; row++) {
		int x1 = area->x + arc_inset(top_left, row);
		int x2 = area->x + area->width - arc_inset(top_right, row);
		if (x2 > x1) {
			boxes[len++] = (pixman_box32_t){ x1, area->y + row, x2, area->y + row + 1 };
		}
	}
	if (area->height > top + bottom) {
		boxes[len++] = (pixman_box32_t){ area->x, area->y + top,
			area->x + area->width, area->y + area->height - bottom };
	}
	for (int row = 0; row < bottom; row++) {
		int y = area->y + area->height - 1 - row;
		int x1 = area->x + arc_inset(bottom_left, row);
		int x2 = area->x + area->width - arc_inset(bottom_right, row);
		if (x2 > x1) {
			boxes[len++] = (pixman_box32_t){ x1, y, x2, y + 1 };
		}
	}

	pixman_region32_init_rects(region, boxes, len);
	free(boxes);
}

struct clipped_region clipped_region_get_default(void) {
	return (struct clipped_region) {
		.corners = {0},
		.area = (struct wlr_box) {0},
	};
}
