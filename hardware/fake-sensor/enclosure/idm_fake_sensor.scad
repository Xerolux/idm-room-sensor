board_w=64; board_h=42; wall=2; clearance=1.5; base_d=9; lid_d=13;
outer_w=board_w+2*clearance+2*wall; outer_h=board_h+2*clearance+2*wall;
module rb(w,h,d,r=2){minkowski(){cube([w-2*r,h-2*r,d],center=true);cylinder(r=r,h=.01,$fn=32);}}
module base(){difference(){translate([0,0,base_d/2])rb(outer_w,outer_h,base_d,2);translate([0,0,base_d/2+wall])cube([outer_w-2*wall,outer_h-2*wall,base_d],center=true);}}
module lid(){difference(){translate([0,0,lid_d/2])rb(outer_w,outer_h,lid_d,2);translate([0,0,lid_d/2-wall])cube([outer_w-2*wall,outer_h-2*wall,lid_d],center=true);for(y=[-outer_h/2+6:5:outer_h/2-6])translate([0,y,lid_d-wall/2])cube([outer_w-12,2.5,wall+2],center=true);}}
base();
