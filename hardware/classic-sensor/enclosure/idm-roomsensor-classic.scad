board_w=38; board_h=28; wall=2; clearance=1.5; base_d=8; lid_d=12;
outer_w=board_w+2*clearance+2*wall; outer_h=board_h+2*clearance+2*wall;
module rounded_box(w,h,d,r=2){minkowski(){cube([w-2*r,h-2*r,d],center=true);cylinder(r=r,h=.01,$fn=32);}}
module base(){difference(){translate([0,0,base_d/2])rounded_box(outer_w,outer_h,base_d,2);translate([0,0,base_d/2+wall])cube([outer_w-2*wall,outer_h-2*wall,base_d],center=true);}}
module lid(){difference(){translate([0,0,lid_d/2])rounded_box(outer_w,outer_h,lid_d,2);translate([0,0,lid_d/2-wall])cube([outer_w-2*wall,outer_h-2*wall,lid_d],center=true);for(y=[-outer_h/2+6:5:outer_h/2-6])translate([0,y,lid_d-wall/2])cube([outer_w-10,2.5,wall+2],center=true);}}
base();
