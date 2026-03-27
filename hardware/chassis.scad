// ============================================================================
// Car DIY - 3D Printable Chassis
// 2 front driving wheels (JGA25-370 with L-brackets) + 1 rear caster
// ============================================================================
// Print: 0.2mm layer, 20% infill, PLA or PETG
// Orientation: print flat (base plate down)
// ============================================================================

/* [Main Dimensions] */
// Total chassis length (front to back)
chassis_length = 180;
// Total chassis width (must fit both motors + wheels)
chassis_width = 140;
// Base plate thickness
base_thickness = 3;
// Wall height
wall_height = 25;
// Wall thickness
wall_thickness = 2.5;

/* [JGA25-370 Motor] */
// Motor body diameter
motor_dia = 25;
// Total motor length (body + gearbox + encoder)
motor_total_length = 58;
// Shaft center height from bracket base
motor_center_h = 14.5;  // half of 25mm + bracket thickness
// L-bracket screw holes spacing
bracket_hole_spacing = 17;
// L-bracket screw diameter (M3)
bracket_screw_dia = 3.2;
// L-bracket base width
bracket_width = 26;
// L-bracket base depth (how far from wall)
bracket_depth = 34;
// L-bracket height (vertical part)
bracket_height = 33.5;
// Distance from chassis edge to motor center
motor_inset = 40;  // enough for wheel + bracket

/* [Wheel Clearance] */
// Wheel diameter (typical for JGA25-370)
wheel_dia = 65;
// Wheel width
wheel_width = 28;
// Clearance between wheel and chassis wall
wheel_clearance = 3;

/* [L298N Module] */
l298n_length = 44;
l298n_width = 44;
l298n_hole_spacing_x = 37;
l298n_hole_spacing_y = 37;
l298n_screw_dia = 3.2;
l298n_standoff_h = 8;

/* [ESP32-CAM] */
esp32_length = 40.5;
esp32_width = 27;

/* [Arduino Pro Mini] */
arduino_length = 33;
arduino_width = 18;

/* [Battery] */
// 3S LiPo approximate size
battery_length = 105;
battery_width = 35;
battery_height = 25;

/* [Caster Wheel] */
// 12mm ball caster mounting
caster_hole_spacing = 16;
caster_screw_dia = 2.5;
caster_installed_h = 15;  // must match front wheel center height

/* [Computed Values] */
// Front motor mount Y positions (symmetric)
motor_y_left = chassis_width/2 - motor_inset;
motor_y_right = -(chassis_width/2 - motor_inset);
// Front X position
motor_x = chassis_length/2 - motor_total_length/2 - 10;

// ============================================================================
// Modules
// ============================================================================

module base_plate() {
    // Main base plate with rounded corners
    hull() {
        for (x = [-chassis_length/2 + 8, chassis_length/2 - 8])
            for (y = [-chassis_width/2 + 8, chassis_width/2 - 8])
                translate([x, y, 0])
                    cylinder(r=8, h=base_thickness, $fn=32);
    }
}

module walls() {
    difference() {
        // Outer walls
        hull() {
            for (x = [-chassis_length/2 + 8, chassis_length/2 - 8])
                for (y = [-chassis_width/2 + 8, chassis_width/2 - 8])
                    translate([x, y, 0])
                        cylinder(r=8, h=wall_height, $fn=32);
        }
        // Hollow inside
        translate([0, 0, base_thickness])
        hull() {
            for (x = [-chassis_length/2 + 8 + wall_thickness, chassis_length/2 - 8 - wall_thickness])
                for (y = [-chassis_width/2 + 8 + wall_thickness, chassis_width/2 - 8 - wall_thickness])
                    translate([x, y, 0])
                        cylinder(r=8, h=wall_height + 1, $fn=32);
        }
    }
}

module wheel_cutout(y_pos) {
    // Cutout in the wall for the wheel to protrude
    cutout_width = wheel_width + wheel_clearance * 2;
    cutout_height = wheel_dia/2 + 10;
    cutout_length = wheel_dia + 10;

    translate([motor_x, y_pos > 0 ? chassis_width/2 - wall_thickness - 1 : -chassis_width/2 - 1, base_thickness])
        cube([cutout_length, wall_thickness + 2, cutout_height]);
}

module motor_bracket_holes(y_pos) {
    // L-bracket mounting holes on the base plate
    // Two M3 holes spaced 17mm apart, centered on motor axis
    mirror_y = y_pos > 0 ? 1 : -1;

    // Bracket mounts on the base plate, motor axis is parallel to Y
    // Holes go through the base plate
    for (dx = [-bracket_hole_spacing/2, bracket_hole_spacing/2]) {
        translate([motor_x + dx, y_pos, -1])
            cylinder(d=bracket_screw_dia, h=base_thickness + 2, $fn=24);
    }

    // Additional bracket holes for the vertical part (optional mounting)
    for (dx = [-bracket_hole_spacing/2, bracket_hole_spacing/2]) {
        translate([motor_x + dx, y_pos, -1])
            cylinder(d=bracket_screw_dia, h=base_thickness + 2, $fn=24);
    }
}

module motor_mount_platform(y_pos) {
    // Raised platform for L-bracket mounting
    platform_w = bracket_width + 6;
    platform_d = bracket_depth + 6;

    translate([motor_x - platform_w/2, y_pos - platform_d/2, base_thickness])
        cube([platform_w, platform_d, 2]);
}

module l298n_mount() {
    // L298N mounting position: center of chassis
    l298n_x = -10;
    l298n_y = 0;

    // Standoff posts
    for (dx = [-l298n_hole_spacing_x/2, l298n_hole_spacing_x/2])
        for (dy = [-l298n_hole_spacing_y/2, l298n_hole_spacing_y/2])
            translate([l298n_x + dx, l298n_y + dy, base_thickness]) {
                difference() {
                    cylinder(d=7, h=l298n_standoff_h, $fn=24);
                    translate([0, 0, -0.5])
                        cylinder(d=l298n_screw_dia, h=l298n_standoff_h + 1, $fn=24);
                }
            }

    // Label
    // translate([l298n_x, l298n_y, base_thickness])
    //     %cube([l298n_length, l298n_width, 2], center=true);  // ghost for reference
}

module esp32_mount() {
    // ESP32-CAM mount: front-center, camera facing forward
    esp_x = chassis_length/2 - 15;
    esp_y = 0;

    // Simple slot/clip mount
    slot_w = esp32_width + 1;
    slot_l = esp32_length + 1;

    // Side rails to hold the board
    for (dy = [-slot_w/2 - 1.5, slot_w/2 + 0.5])
        translate([esp_x - slot_l/2, esp_y + dy, base_thickness])
            cube([slot_l, 1, 8]);

    // Back stop
    translate([esp_x - slot_l/2 - 1, esp_y - slot_w/2 - 1.5, base_thickness])
        cube([1, slot_w + 3, 8]);
}

module camera_hole() {
    // Hole in front wall for camera lens
    translate([chassis_length/2 - wall_thickness - 1, -8, base_thickness + 5])
        cube([wall_thickness + 2, 16, 12]);
}

module arduino_mount() {
    // Arduino Pro Mini: near L298N
    ard_x = -35;
    ard_y = 25;

    // Pin header standoffs
    for (dx = [-arduino_length/2 + 3, arduino_length/2 - 3])
        for (dy = [-arduino_width/2 + 3, arduino_width/2 - 3])
            translate([ard_x + dx, ard_y + dy, base_thickness])
                cylinder(d=5, h=4, $fn=24);
}

module battery_bay() {
    // Battery compartment: rear-center, with strap slots
    bat_x = -chassis_length/2 + battery_length/2 + 15;
    bat_y = 0;

    // Retaining walls (partial, for strap access)
    for (dy = [-battery_width/2 - 2, battery_width/2 + 1])
        translate([bat_x - battery_length/2, bat_y + dy, base_thickness])
            cube([battery_length, 1, battery_height/2]);

    // Front/back stops
    for (dx = [-battery_length/2 - 1, battery_length/2])
        translate([bat_x + dx, bat_y - battery_width/2 - 2, base_thickness])
            cube([1, battery_width + 4, battery_height/2]);

    // Strap slots (through base plate)
    for (dx = [-battery_length/3, battery_length/3])
        translate([bat_x + dx, bat_y - battery_width/2 - 5, -1])
            cube([3, battery_width + 10, base_thickness + 2]);
}

module caster_mount() {
    // Rear caster wheel mount: center-back
    caster_x = -chassis_length/2 + 20;
    caster_y = 0;

    // Mounting holes for ball caster
    for (dx = [-caster_hole_spacing/2, caster_hole_spacing/2])
        for (dy = [-caster_hole_spacing/2, caster_hole_spacing/2])
            translate([caster_x + dx, caster_y + dy, -1])
                cylinder(d=caster_screw_dia, h=base_thickness + 2, $fn=24);

    // Reinforcement pad
    translate([caster_x, caster_y, 0])
        cylinder(d=caster_hole_spacing + 10, h=base_thickness, $fn=48);
}

module cable_channels() {
    // Channel from motor area to center (for encoder cables)
    for (y_pos = [motor_y_left, motor_y_right]) {
        translate([motor_x - 5, y_pos > 0 ? 15 : -20, base_thickness])
            cube([10, 5, 2]);
    }
}

module general_mounting_holes() {
    // M3 mounting holes in corners for accessories/top plate
    corner_inset = 15;
    for (x = [-chassis_length/2 + corner_inset, chassis_length/2 - corner_inset])
        for (y = [-chassis_width/2 + corner_inset, chassis_width/2 - corner_inset])
            translate([x, y, -1])
                cylinder(d=3.2, h=base_thickness + 2, $fn=24);
}

module switch_hole() {
    // Power switch mounting hole on side wall
    translate([-chassis_length/2 + 50, chassis_width/2 - wall_thickness - 1, base_thickness + 8])
        cube([12, wall_thickness + 2, 8]);
}

module wire_passthrough() {
    // Holes in internal walls for wiring
    // Motor power from L298N to front motors
    for (y_pos = [25, -25])
        translate([20, y_pos, base_thickness + 3])
            rotate([0, 90, 0])
                cylinder(d=8, h=20, $fn=24);
}

// ============================================================================
// Assembly
// ============================================================================

module chassis() {
    difference() {
        union() {
            // Base structure
            walls();

            // Motor mount platforms
            motor_mount_platform(motor_y_left);
            motor_mount_platform(motor_y_right);

            // L298N standoffs
            l298n_mount();

            // ESP32-CAM mount
            esp32_mount();

            // Arduino mount
            arduino_mount();

            // Battery bay
            battery_bay();

            // Caster reinforcement
            caster_mount();
        }

        // Subtractive features
        // Wheel cutouts in walls
        wheel_cutout(motor_y_left);
        wheel_cutout(motor_y_right);

        // Motor bracket screw holes
        motor_bracket_holes(motor_y_left);
        motor_bracket_holes(motor_y_right);

        // Camera hole
        camera_hole();

        // Caster screw holes (already in caster_mount but need to go through)
        caster_x = -chassis_length/2 + 20;
        for (dx = [-caster_hole_spacing/2, caster_hole_spacing/2])
            for (dy = [-caster_hole_spacing/2, caster_hole_spacing/2])
                translate([caster_x + dx, 0 + dy, -1])
                    cylinder(d=caster_screw_dia, h=base_thickness + 10, $fn=24);

        // General mounting holes
        general_mounting_holes();

        // Switch hole
        switch_hole();
    }
}

// Render
chassis();

// ============================================================================
// Ghost reference objects (uncomment to visualize component placement)
// ============================================================================

// Motors (ghost)
%translate([motor_x, motor_y_left, base_thickness + motor_center_h])
    rotate([0, 90, 0])
        cylinder(d=motor_dia, h=motor_total_length, center=true, $fn=32);

%translate([motor_x, motor_y_right, base_thickness + motor_center_h])
    rotate([0, 90, 0])
        cylinder(d=motor_dia, h=motor_total_length, center=true, $fn=32);

// Wheels (ghost)
%translate([motor_x, motor_y_left + motor_dia/2 + 5, base_thickness + motor_center_h])
    rotate([90, 0, 0])
        cylinder(d=wheel_dia, h=wheel_width, center=true, $fn=48);

%translate([motor_x, motor_y_right - motor_dia/2 - 5, base_thickness + motor_center_h])
    rotate([90, 0, 0])
        cylinder(d=wheel_dia, h=wheel_width, center=true, $fn=48);

// Battery (ghost)
%translate([-chassis_length/2 + battery_length/2 + 15, 0, base_thickness + battery_height/2])
    cube([battery_length, battery_width, battery_height], center=true);

// L298N (ghost)
%translate([-10, 0, base_thickness + l298n_standoff_h + 1])
    cube([l298n_length, l298n_width, 2], center=true);
