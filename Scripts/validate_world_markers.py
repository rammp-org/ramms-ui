"""
Validate URammsDetectionWorldMarkers against synthetic data, inside a running PIE session.

Run it through the editor's remote execution, with PIE already playing:

    python3 <ramms-sim>/Scripts/editor_remote_exec.py --project RammpUI \
        --file Plugins/RammsUI/Scripts/validate_world_markers.py

Two independent checks:

  PRODUCER PATH   A payload carrying position_m / size_m / orientation is resolved and compared
                  against hand-computed expectations. Needs no camera at all.

  DERIVED PATH    A synthetic colour+depth camera is registered, a depth buffer is written, and
                  the component is asked to place 2D-only detections from it. Ground truth is
                  exact because the test runs BACKWARDS: a pixel and a depth are chosen, the world
                  point they must correspond to is computed here, and the component has to return
                  it. Nothing is rendered, so there is nothing to eyeball and nothing to drift.

The derived probes span the centre, the edges and the four corners deliberately. Confusing an
axial depth (what RammsProjection.ush means by Depth, and what this reads) for a radial range is
nearly invisible at the image centre and worst at the corners, so a centre-only probe would pass
whichever convention the code had picked.

Stream ids are unique per run. Synthetic providers created by an earlier run stay alive in the PIE
world, and FindDepthSibling takes the first provider that knows a stream id -- so fixed ids make a
second run silently sample a stale provider's empty depth buffer and report nothing resolved. That
is a property of the test rig, not of the component.
"""

import math
import struct
import time

import unreal

CW, CH = 640, 480  # colour resolution
DW, DH = 320, 240  # depth resolution: deliberately different, so normalized coords must carry
FX, FY, CX, CY = 525.0, 525.0, 320.0, 240.0
TOLERANCE_UU = 4.0

# Centre, four edges, four corners.
PROBES = [
	(0.50, 0.50, 180.0),
	(0.08, 0.08, 220.0),
	(0.92, 0.08, 150.0),
	(0.08, 0.92, 260.0),
	(0.92, 0.92, 300.0),
	(0.50, 0.10, 120.0),
	(0.50, 0.90, 400.0),
	(0.06, 0.50, 350.0),
	(0.94, 0.50, 200.0),
]


def _host_actor(world):
	return unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)[0]


def _stream_info(stream_id, width, height, role, depth_format, group, intrinsics):
	info = unreal.RammsCameraStreamInfo()
	info.set_editor_property("stream_id", stream_id)
	info.set_editor_property("width", width)
	info.set_editor_property("height", height)
	info.set_editor_property("stream_role", role)
	info.set_editor_property("group_id", group)
	info.set_editor_property("depth_format", depth_format)
	if intrinsics:
		info.set_editor_property("intrinsics", intrinsics)
	return info


def _depth_buffer(depth_format, width, height, fills):
	"""Fill a patch at each (norm_u, norm_v) with its depth; everything else reads as no-return.

	A patch rather than one pixel because the component takes the median of a window, which is what
	keeps a real depth image's dropouts and edges from placing a marker inside neither surface.
	"""
	if depth_format == unreal.RammsDepthFormat.FLOAT32CM:
		buffer = bytearray(width * height * 4)

		def put(index, depth):
			struct.pack_into("<f", buffer, index * 4, depth)
	else:
		buffer = bytearray(width * height * 2)

		def put(index, depth):
			struct.pack_into("<H", buffer, index * 2, max(0, min(65535, int(round(depth * 10.0)))))

	for norm_u, norm_v, depth in fills:
		px = max(0, min(width - 1, int(norm_u * width)))
		py = max(0, min(height - 1, int(norm_v * height)))
		for y in range(max(0, py - 3), min(height, py + 4)):
			for x in range(max(0, px - 3), min(width, px + 4)):
				put(y * width + x, depth)
	return bytes(buffer)


def _distance(a, b):
	return math.sqrt((a.x - b.x) ** 2 + (a.y - b.y) ** 2 + (a.z - b.z) ** 2)


def check_producer_path(world):
	"""A sent position, size and orientation, against values worked out by hand."""
	component = unreal.new_object(unreal.RammsDetectionWorldMarkers, outer=_host_actor(world))
	component.set_editor_property("draw_debug_markers", False)
	component.set_detected_objects_json(
		'{"stream_id":"test/color","selected_instance_id":"fridge_1","objects":['
		'{"instance_id":"fridge_1","perception_class":"fridge","frame":"world",'
		' "position_m":{"X":2.0,"Y":1.0,"Z":0.9},"size_m":{"X":0.60,"Y":0.70,"Z":1.80},'
		' "orientation":{"X":0,"Y":0,"Z":0.3826834,"W":0.9238795}},'
		'{"instance_id":"cup_7","perception_class":"cup","frame":"world",'
		' "position_m":{"X":1.0,"Y":0.2,"Z":0.95}},'
		'{"instance_id":"only_2d","perception_class":"plate","box":[0.1,0.1,0.2,0.2]}]}'
	)

	points = {str(p.instance_id): p for p in component.get_world_points()}
	failures = []

	# Metres to Unreal units is 100x, and the producer's frame is right-handed, so Y negates.
	fridge = points.get("fridge_1")
	if not fridge:
		failures.append("fridge_1 was not resolved")
	else:
		if _distance(fridge.world_location, unreal.Vector(200.0, -100.0, 90.0)) > 0.01:
			failures.append(f"fridge_1 location {fridge.world_location}, expected (200, -100, 90)")
		if _distance(fridge.world_size, unreal.Vector(60.0, 70.0, 180.0)) > 0.01:
			failures.append(f"fridge_1 size {fridge.world_size}, expected (60, 70, 180)")
		# +45 degrees right-handed about Z is -45 in Unreal's left-handed frame.
		if abs(fridge.world_rotation.yaw + 45.0) > 0.01:
			failures.append(f"fridge_1 yaw {fridge.world_rotation.yaw}, expected -45")
		if not fridge.selected:
			failures.append("fridge_1 was named selected_instance_id but is not marked selected")
		if not fridge.position_from_producer:
			failures.append("fridge_1 position should be marked as the producer's")

	cup = points.get("cup_7")
	if not cup:
		failures.append("cup_7 was not resolved")
	elif cup.has_size:
		failures.append("cup_7 sent no size_m, so it must not claim one")

	if "only_2d" in points:
		failures.append("only_2d carries no 3D information and no depth camera exists, so it must be ignored")

	return failures


def check_camera_frame_box(world):
	"""A box sent in CAMERA coordinates: position, extents and rotation all follow the same basis.

	Separate from check_producer_path because that one sends `frame: "world"`, whose axes are
	Unreal's up to the sign of Y -- the one frame where an un-remapped extent happens to be right.
	The optical frame is X right, Y down, Z range, so a 1.80 m "tall" fridge sent here must come
	back 1.80 m along Unreal's X (forward), not its Z.
	"""
	host = _host_actor(world)
	suffix = f"{int(time.time() * 1000) % 100000000}_camframe"
	colour_id, group = f"synth/{suffix}/color", f"synth_{suffix}"

	provider = unreal.new_object(unreal.RammsCameraProviderComponent, outer=host)
	provider.register_stream(
		_stream_info(colour_id, CW, CH, unreal.RammsStreamRole.COLOR, unreal.RammsDepthFormat.UNKNOWN, group, [FX, FY, CX, CY]), True)
	# Translated and rotated, so an extrinsic applied on the wrong side cannot pass.
	cam_rot = unreal.Rotator(0.0, 25.0, -10.0)   # (roll, pitch, yaw) in Python
	camera = unreal.Transform(unreal.Vector(-150.0, 40.0, 110.0), cam_rot, unreal.Vector(1.0, 1.0, 1.0))
	provider.update_stream_extrinsic(colour_id, camera)

	px, py, pz = 0.30, -0.12, 2.40          # metres, optical: right, down, range
	sx, sy, sz = 0.60, 1.80, 0.70           # metres, optical: width, height, depth
	component = unreal.new_object(unreal.RammsDetectionWorldMarkers, outer=host)
	component.set_editor_property("draw_debug_markers", False)
	component.set_detected_objects_json(
		'{"stream_id":"%s","objects":[{"instance_id":"fridge_cam","perception_class":"fridge",'
		' "frame":"camera","position_m":{"X":%.4f,"Y":%.4f,"Z":%.4f},'
		' "size_m":{"X":%.4f,"Y":%.4f,"Z":%.4f},'
		' "orientation":{"X":0,"Y":0,"Z":0,"W":1}}]}' % (colour_id, px, py, pz, sx, sy, sz)
	)

	points = {str(p.instance_id): p for p in component.get_world_points()}
	failures = []
	point = points.get("fridge_cam")
	if not point:
		return ["fridge_cam was not resolved (no extrinsic for the synthetic camera?)"]

	# Optical -> Unreal local is (Z, X, -Y), the same map CameraPointToWorld uses, then the pose.
	expected_loc = camera.transform_location(unreal.Vector(pz * 100.0, px * 100.0, -py * 100.0))
	if _distance(point.world_location, expected_loc) > 0.01:
		failures.append(f"fridge_cam location {point.world_location}, expected {expected_loc}")

	# The same map without the sign, because an extent has no sign: (Z, X, Y).
	expected_size = unreal.Vector(sz * 100.0, sx * 100.0, sy * 100.0)
	if _distance(point.world_size, expected_size) > 0.01:
		failures.append(f"fridge_cam size {point.world_size}, expected {expected_size}")

	# An identity rotation in the camera's frame is the camera's own orientation, nothing more.
	delta = unreal.MathLibrary.normalized_delta_rotator(point.world_rotation, cam_rot)
	if max(abs(delta.pitch), abs(delta.yaw), abs(delta.roll)) > 0.05:
		failures.append(f"fridge_cam rotation {point.world_rotation}, expected the camera's {cam_rot}")

	if not point.has_size or not point.has_rotation or not point.position_from_producer:
		failures.append("fridge_cam sent position, size and orientation; all three must be flagged as the producer's")

	return failures


def check_derived_path(world, depth_format, format_name):
	"""A synthetic camera, and probes whose true world positions are known by construction."""
	host = _host_actor(world)
	suffix = f"{int(time.time() * 1000) % 100000000}_{format_name}"
	colour_id, depth_id, group = f"synth/{suffix}/color", f"synth/{suffix}/depth", f"synth_{suffix}"

	provider = unreal.new_object(unreal.RammsCameraProviderComponent, outer=host)
	provider.register_stream(
		_stream_info(colour_id, CW, CH, unreal.RammsStreamRole.COLOR, unreal.RammsDepthFormat.UNKNOWN, group, [FX, FY, CX, CY]), True)
	provider.register_stream(
		_stream_info(depth_id, DW, DH, unreal.RammsStreamRole.DEPTH, depth_format, group, []), True)

	# Translated and rotated: an identity pose would hide an extrinsic applied on the wrong side.
	camera = unreal.Transform(
		unreal.Vector(-150.0, 40.0, 110.0), unreal.Rotator(-10.0, 25.0, 0.0), unreal.Vector(1.0, 1.0, 1.0))
	provider.update_stream_extrinsic(colour_id, camera)
	provider.update_stream_extrinsic(depth_id, camera)
	provider.broadcast_frame_data(depth_id, _depth_buffer(depth_format, DW, DH, PROBES), DW, DH)

	# The world point each probe must resolve to. This is the pinhole model inverted here, in a
	# separate implementation from the component's, so agreement means something.
	truth = []
	for norm_u, norm_v, depth in PROBES:
		cam_x = (norm_u * CW - CX) * depth / FX
		cam_y = (norm_v * CH - CY) * depth / FY
		truth.append(camera.transform_location(unreal.Vector(depth, cam_x, -cam_y)))

	component = unreal.new_object(unreal.RammsDetectionWorldMarkers, outer=host)
	component.set_editor_property("draw_debug_markers", False)
	objects = ",".join(
		'{"instance_id":"probe_%d","perception_class":"probe","source":"%s","centroid":[%.6f,%.6f]}'
		% (i, colour_id, probe[0], probe[1])
		for i, probe in enumerate(PROBES)
	)
	component.set_detected_objects_json('{"stream_id":"%s","objects":[%s]}' % (colour_id, objects))

	resolved = {str(p.instance_id): p for p in component.get_world_points()}
	failures, worst = [], 0.0
	for i, expected in enumerate(truth):
		point = resolved.get(f"probe_{i}")
		if point is None:
			failures.append(f"probe_{i} at {PROBES[i][:2]} resolved no range")
			continue
		if point.position_from_producer:
			failures.append(f"probe_{i} sent no position_m, so it must be marked as derived")
		error = _distance(point.world_location, expected)
		worst = max(worst, error)
		if error > TOLERANCE_UU:
			failures.append(f"probe_{i} at {PROBES[i][:2]}: off by {error:.2f} UU")
	return failures, worst


def main():
	world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
	if not world:
		print("FAIL: no PIE session. Press Play first; the component resolves against a live world.")
		return

	all_failures = []

	failures = check_producer_path(world)
	print(f"producer path (position_m / size_m / orientation): {'PASS' if not failures else 'FAIL'}")
	all_failures += failures

	failures = check_camera_frame_box(world)
	print(f"camera-frame box (position, extents and rotation through one basis): {'PASS' if not failures else 'FAIL'}")
	all_failures += failures

	for depth_format, name in ((unreal.RammsDepthFormat.FLOAT32CM, "Float32CM"), (unreal.RammsDepthFormat.UINT16MM, "Uint16MM")):
		failures, worst = check_derived_path(world, depth_format, name)
		print(f"derived path, {name} depth, {len(PROBES)} probes across the frame: "
			  f"{'PASS' if not failures else 'FAIL'} (worst {worst:.2f} UU, tolerance {TOLERANCE_UU})")
		all_failures += failures

	print()
	if all_failures:
		print(f"{len(all_failures)} FAILURE(S):")
		for failure in all_failures:
			print(f"  - {failure}")
	else:
		print("ALL PASS")


main()
