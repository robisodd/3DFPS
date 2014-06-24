/*
  *********************************************************************************
   Pebble 3D FPS Engine v0.2b
   Created by Rob Spiess (robisodd@gmail.com) on June 23, 2014
  *********************************************************************************
  v0.1b: Initial Release
  v0.2b: Converted all floating points to int32_t which increased framerate substancially
         Gets overflow errors, so when a number squared is negative, sets it to 2147483647.
         Added door blocks for another example
  *********************************************************************************
  Created by reading this website: http://www.playfuljs.com/a-first-person-engine-in-265-lines/

  CC Copyright (c) 2014 All Right Reserved
  THIS CODE AND INFORMATION ARE PROVIDED "AS IS" WITHOUT WARRANTY OF ANY 
  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
  http://creativecommons.org/licenses/by/3.0/legalcode
*/

#include "pebble.h"

#define ACCEL_STEP_MS 10       // Update frequency
#define root_depth 20          // How many iterations square root function performs
#define mapsize 20             // Map is 20x20 squres, or whatever number is here
#define range 10000            // Distance player can see
#define idclip false           // Walk thru walls
#define view_border true       // Draw border around viewing window
#define draw_textbox true      // Draw textbox or not
  
//----------------------------------//
// Viewing Window Size and Position //
//----------------------------------//
// beneficial to: (set fov as divisible by view_w) and (have view_w evenly divisible by 2)
// e.g.: view_w=144 is good since 144/2=no remainder. Set fov = 13104fov (since it = 144w x 91 and is close to 20% of 65536)
  
// Full Screen (You should also comment out drawing the text box)
//#define view_x 0             // View Left Edge
//#define view_y 0             // View Top Edge
//#define view_w 144           // View Width in pixels
//#define view_h 168           // View Hight in pixels
//#define fov 13104            // Field of view angle (20% of a circle is good) (TRIG_MAX_RATIO = 0x10000 or 65536) * 20%

// Smaller square
//#define view_x 20            // View Left Edge
//#define view_y 30            // View Top Edge
//#define view_w 100           // View Width in pixels
//#define view_h 100           // View Hight in pixels
//#define fov 13100            // Field of view angle (20% of a circle is good) (TRIG_MAX_RATIO = 0x10000 or 65536) * 20%

//Nearly full screen
#define view_x 1               // View Left Edge
#define view_y 25              // View Top Edge
#define view_w 142             // View Width in pixels
#define view_h 140             // View Hight in pixels
#define fov 13064              // Field of view angle (20% of a circle is good) (TRIG_MAX_RATIO = 0x10000 or 65536) * 20%

//----------------------------------//
#define fov_over_w fov/view_w  // Do math now so less during execution
#define half_view_w view_w/2   //
//----------------------------------//
 
typedef struct PlayerVar {
  int32_t x;                  // Player's X Position x1000
  int32_t y;                  // Player's Y Position x1000
  int32_t facing;           // Player Direction Facing (from 0 - TRIG_MAX_ANGLE)
} PlayerVar;

typedef struct RayVar {
  int32_t x;                  // Origin X
  int32_t y;                  // Origin Y
  int32_t dist;               // Distance
  int8_t hit;                 // Hit (What on the map it hit)
  int32_t offset;             // Offset (used for wall texture)
} RayVar;

static Window *window;
static GRect window_frame;
static Layer *graphics_layer;
static AppTimer *timer;

static int8_t map[mapsize * mapsize];  // int8 means cells can be from -127 to 128
static PlayerVar player;

//float sine_rad(float theta) {return (float)sin_lookup(TRIG_MAX_ANGLE * theta / (MATH_PI * 2)) / (float)TRIG_MAX_RATIO;} //sin(radians=0-2pi)
//float cosine_rad(float theta) {return (float)cos_lookup(TRIG_MAX_ANGLE * theta / (MATH_PI * 2)) / (float)TRIG_MAX_RATIO;} //cos(radians=0-2pi)
//float floor_float(float a){int32_t b=(int32_t)a; if((float)b!=a) if(a<0)b--; return (float)b;}
//float  ceil_float(float a){int32_t b=(int32_t)a; if((float)b!=a) if(a>0)b++; return (float)b;}
int32_t floor_int(int32_t a){int32_t b=(a-a%1000); if(b!=a) if(a<0)b-=1000; return b;}
int32_t  ceil_int(int32_t a){int32_t b=(a-a%1000); if(b!=a) if(a>0)b+=1000; return b;}
int32_t  sqrt_int(int32_t a) {int32_t b=a; for(int8_t i=0; i<root_depth; i++) b=(b+(a/b))/2; return b;} // Square Root

// ------------------------------------------------------------------------ //
//  Map Functions
// ------------------------------------------------------------------------ //
void GenerateMap() {
  for (int16_t i=0; i<mapsize*mapsize; i++) map[i] = rand() % 3 == 0 ? 1 : 0;       // Randomly 1/3 of spots are blocks
  for (int16_t i=0; i<mapsize*mapsize; i++) if(map[i]==1 && rand()%4==0) map[i]=2;  // Changes blocks to stripey blocks
  for (int16_t i=0; i<mapsize*mapsize; i++) if(map[i]==1 && rand()%4==0) map[i]=3;  // Changes blocks to door blocks
}

int8_t getmap(int32_t x, int32_t y) {
  if (x<0 || x>=(mapsize*1000) || y<0 || y>=(mapsize*1000)) return -1;
  return map[(((y - y%1000)/1000) * mapsize) + ((x - x%1000)/1000)];
  return map[((y/1000) * mapsize) + (x/1000)];  // This might work due to integer math dropping remainder
}

/*
// Set map code.  Not used, but could be useful.  :\
void setmap(int16_t x, int16_t y, int8_t value) {
  if ((x >= 0) && (x < mapsize) && (y >= 0) && (y < mapsize))
    map[y * mapsize + x] = value;
}
*/

// ------------------------------------------------------------------------ //

void walk(int32_t distance) {
  int32_t dx = (cos_lookup(player.facing) * distance) / TRIG_MAX_RATIO;
  int32_t dy = (sin_lookup(player.facing) * distance) / TRIG_MAX_RATIO;
  if(getmap(floor_int(player.x + dx), floor_int(player.y)) <= 0 || idclip) player.x += dx;
  if(getmap(floor_int(player.x), floor_int(player.y + dy)) <= 0 || idclip) player.y += dy;
}

static void main_loop(void *data) {
  AccelData accel=(AccelData){.x=0, .y=0, .z=0}; // All three are int16_t
  accel_service_peek(&accel);                    // Read Accelerometer
  player.facing += (10 * accel.x);               // Spin based on accel.x
  walk((int32_t)accel.y);                        // Walk based on accel.y
  //walk(accel.y * 1000 / 1000);        // Technically above line is this
  layer_mark_dirty(graphics_layer);              // Tell pebble to draw when it's ready
}

// ------------------------------------------------------------------------ //

static void graphics_layer_update_proc(Layer *me, GContext *ctx) {
	RayVar ray;
  int32_t coltop, colbot, angle, sin, cos;
  int32_t Xdx=0, Xdy=0, Ydx=0, Ydy=0, z, Xlen, Ylen;
  //int32_t looper = 0;
  z=0; //delete this
  
  time_t sec1, sec2; uint16_t ms1, ms2; int32_t dt; // time snapshot variables
  //time_t sec3; uint16_t ms3; int32_t dt2; // time snapshot variables
  time_ms(&sec1, &ms1);  //1st Time Snapshot

  // Draw black background over whole canvas
   // Not needed.  Maybe draw some sort of other background?
  
  // Draw Box around view (not needed if fullscreen, i.e. view_w=144 and view_h=168)
  if(view_border) {graphics_context_set_stroke_color(ctx, 1); graphics_draw_rect(ctx, GRect(view_x-1, view_y-1, view_w+2, view_h+2));}  //White Rectangle Border
  
  // Bring me the horizon
  graphics_context_set_stroke_color(ctx, 1); graphics_draw_line(ctx, GPoint(view_x, view_y + (int)(view_h/2)), GPoint(view_x + view_w,view_y + (int)(view_h/2)));
   
  // Begin RayTracing Loop
  for(int16_t col = 0; col < view_w; col++) {
    //angle = (int32_t)(fov * (((float)col/view_w) - 0.5));
    angle = fov_over_w * (col - half_view_w);  // Same equation as above, but less math = faster but confusing to people

    sin = sin_lookup(player.facing + angle);
    cos = cos_lookup(player.facing + angle); 
    ray = (RayVar){.x=player.x, .y=player.y, .dist=0};         //Shoot rays out of player's eyes.  pew pew.
    
    bool going = true;  // Loop until something causes you to stop
    while(going) {
      //time_ms(&sec3, &ms3);  //1st Time Snapshot
      //looper++;
      // Calculate distance to next X gridline in the ray's direction
      if (cos == 0) Xlen = 2147483647;  // If ray is vertical, will never hit next X
      else {
	      Xdx = cos > 0 ? floor_int(ray.x + 1000) - ray.x : ceil_int(ray.x - 1000) - ray.x;
        Xdy = (Xdx * sin)/cos;
        Xlen = Xdx * Xdx + Xdy * Xdy;  // Multiplying 2 32bit numbers might overflow
        if(Xlen<0) Xlen = 2147483647;  // Overflow detected so just make length the max
      }

      // Calculate distance to next Y gridline in the ray's direction
	    if (sin == 0) Ylen = 2147483647;    // If ray is horizontal, will never hit next Y
	    else {
        Ydy = sin > 0 ? floor_int(ray.y + 1000) - ray.y : ceil_int(ray.y - 1000) - ray.y;
	      Ydx = (Ydy * cos)/sin;
        Ylen = Ydx * Ydx + Ydy * Ydy;
        if(Ylen<0) Ylen = 2147483647;  // Overflow detected so just make length the max
      }

      // move ray to next step whichever is closer
	    if(Xlen < Ylen) {
        ray.x += Xdx;
        ray.y += Xdy;
	      ray.hit = getmap(floor_int(ray.x - (cos<0?1000:0)), floor_int(ray.y));
        ray.dist = ray.dist + sqrt_int(Xlen);
	      ray.offset = ray.y;
      } else {
        ray.x += Ydx;
        ray.y += Ydy;
        ray.hit = getmap(floor_int(ray.x),floor_int(ray.y - (sin<0?1000:0)));
        ray.dist = ray.dist + sqrt_int(Ylen);
	      ray.offset = ray.x;
	    }

	    if (ray.hit > 0) {	   // if ray hits a wall
        going = false;       // stop ray
        ray.offset = ray.offset%1000;  // Get fractional part of offset: offset is where on wall ray hits: 000(left) to 999(right)
        
        z = (ray.dist * cos_lookup(angle)) / TRIG_MAX_RATIO;  //z = distance
        colbot = ((view_h/2) * (z+1000)) / z;  // y coordinate of bottom of column to draw
        coltop = colbot - ((view_h*1000) / z);   // y coordinate of top of column to draw
        
        if(colbot>=view_h) colbot=view_h-1; if(coltop<0) coltop=0;  // Make sure line isn't drawn beyond bounding box
        
        
        
        // Draw Wall Column
        graphics_context_set_stroke_color(ctx, 1);  // Black = 0, White = 1
        if(ray.offset<50 || ray.offset > 950) graphics_context_set_stroke_color(ctx, 0);  // Black edges on left and right 5% of block (Comment this line to remove edges)
        //Note: Uncomment out line below for stripey blocks.  The "*9)%2" means 9 Stripes and 2 is every other.
/*
        if(ray.hit==2){if(floor_int((ray.offset*9)%2000)== 0) graphics_context_set_stroke_color(ctx, 1); else graphics_context_set_stroke_color(ctx, 0);}  // Stripey Blocks
        if(ray.hit==3){if(ray.offset>250 && ray.offset<750){ // If door block and if on door part
            graphics_context_set_stroke_color(ctx, 0); graphics_draw_line(ctx, GPoint((int)col + view_x,coltop + ((colbot-coltop)/3) + view_y), GPoint((int)col + view_x,colbot + view_y));  //bottom third black
            colbot = coltop + ((colbot-coltop)/3); graphics_context_set_stroke_color(ctx, 1);  // Draw white top third
        } }
        */
        //graphics_draw_line(ctx, GPoint((int)col + view_x,coltop + view_y), GPoint((int)col + view_x,colbot + view_y));  //Draw the line
        
        if(ray.offset<50 || ray.offset > 950){
          graphics_context_set_stroke_color(ctx, 0);
          graphics_draw_line(ctx, GPoint((int)col + view_x,coltop + view_y), GPoint((int)col + view_x,colbot + view_y));  //Draw the line
        } else {
          coltop += view_y;
          colbot += view_y - coltop;

        z=sqrt_int(z/2) / 7;  //z = 0 to 10: 0=close 10=distant
        for(int16_t i=0; i<colbot;i++) {
          if((i+ray.offset)%9>=z) graphics_context_set_stroke_color(ctx, 1); else graphics_context_set_stroke_color(ctx, 0);
          if(rand()%z==0) graphics_context_set_stroke_color(ctx, 1); else graphics_context_set_stroke_color(ctx, 0);
          graphics_draw_pixel(ctx, GPoint(col, i+coltop));
        }
        }
        //graphics_draw_line(ctx, GPoint((int)col + view_x,coltop + view_y), GPoint((int)col + view_x,colbot + view_y));  //Draw the line
	    }

      if((sin<0&&ray.y<0)||(sin>0&&ray.y>=(mapsize*1000))||(cos<0&&ray.x<0)||(cos>0&&ray.x>=(mapsize*1000))) going=false;  // stop if ray is out of bounds AND going wrong way
      if(ray.dist > range) going=false;  // Stop ray after traveling too far
    } //End While
  } //End For (End RayTracing Loop)

  time_ms(&sec2, &ms2);  //2nd Time Snapshot
  dt = ((int32_t)1000*(int32_t)sec2 + (int32_t)ms2) - ((int32_t)1000*(int32_t)sec1 + (int32_t)ms1);  //ms between two time snapshots
  //dt2 = ((int32_t)1000*(int32_t)sec2 + (int32_t)ms2) - ((int32_t)1000*(int32_t)sec3 + (int32_t)ms3);  //ms between two time snapshots
  
  //-----------------//
  // Display TextBox //
  //-----------------//
  if(draw_textbox) {
    GRect textframe = GRect(0, 0, 143, 20);  // Text Box Position and Size
    graphics_context_set_fill_color(ctx, 0);   graphics_fill_rect(ctx, textframe, 0, GCornerNone);  //Black Solid Rectangle
    graphics_context_set_stroke_color(ctx, 1); graphics_draw_rect(ctx, textframe);                //White Rectangle Border  
    static char text[40];  //Buffer to hold text
    snprintf(text, sizeof(text), " (%d.%d,%d.%d) %dms %dfps", (int)(floor_int(player.x)/1000),(int)(((player.x<0?(-1*player.x):player.x)%1000)/100), (int)(floor_int(player.y)/1000),(int)(((player.y<0?(-1*player.y):player.y)%1000)/100),(int)dt, (int)(1000/dt));  // What text to draw  
    snprintf(text, sizeof(text), " (%d)", (int)z);
    //snprintf(text, sizeof(text), " (%d) %dms %dms", (int)looper, (int)dt, (int)dt2);  // What text to draw  
    graphics_context_set_text_color(ctx, 1);  // White Text
    graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_14), textframe, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);  //Write Text
  }
  
  //Done.  Set a timer to restart loop
  timer = app_timer_register(ACCEL_STEP_MS, main_loop, NULL);
  
  // Perhaps clean up variables here?
}

// ------------------------------------------------------------------------ //

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  window_frame = layer_get_frame(window_layer);

  graphics_layer = layer_create(window_frame);
  layer_set_update_proc(graphics_layer, graphics_layer_update_proc);
  layer_add_child(window_layer, graphics_layer);
}

static void window_unload(Window *window) {
  layer_destroy(graphics_layer);
}

static void init(void) {
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });
  window_set_fullscreen(window, true);  // Get rid of the top bar
  window_stack_push(window, false /* False = Not Animated */);
  window_set_background_color(window, GColorBlack);
  accel_data_service_subscribe(0, NULL);  // Start accelerometer
  
  srand(time(NULL));  // Seed randomizer so different map every time
  player = (PlayerVar){.x=5000, .y=-2000, .facing=10000};  // Seems like a good place to start
  GenerateMap();      // Randomly generate a map
  
  timer = app_timer_register(ACCEL_STEP_MS, main_loop, NULL);  // Begin main loop
}

static void deinit(void) {
  accel_data_service_unsubscribe();
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}