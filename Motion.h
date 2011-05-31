#ifndef _MOTION_H_
#define _MOTION_H_

#include "config.h"
#include "Movedata.h"
#include "MGcode.h"
#include "Axis.h"
#include "Globals.h"

class Motion
{
  // Singleton

  
public:
  static Motion& Instance() { static Motion instance; return instance; };
private:
  explicit Motion() :AXES((Axis[NUM_AXES])
  {
    Axis(X_STEP_PIN,X_DIR_PIN,X_ENABLE_PIN,X_MIN_PIN,X_MAX_PIN,X_STEPS_PER_UNIT,X_INVERT_DIR,X_LENGTH,X_MAX_FEED,X_AVG_FEED,X_START_FEED,X_ACCEL_DIST,X_HOME_DIR),
    Axis(Y_STEP_PIN,Y_DIR_PIN,Y_ENABLE_PIN,Y_MIN_PIN,Y_MAX_PIN,Y_STEPS_PER_UNIT,Y_INVERT_DIR,Y_LENGTH,Y_MAX_FEED,Y_AVG_FEED,Y_START_FEED,Y_ACCEL_DIST,Y_HOME_DIR),
    Axis(Z_STEP_PIN,Z_DIR_PIN,Z_ENABLE_PIN,Z_MIN_PIN,Z_MAX_PIN,Z_STEPS_PER_UNIT,Z_INVERT_DIR,Z_LENGTH,Z_MAX_FEED,Z_AVG_FEED,Z_START_FEED,Z_ACCEL_DIST,Z_HOME_DIR),
    Axis(A_STEP_PIN,A_DIR_PIN,A_ENABLE_PIN,Pin(),Pin(),A_STEPS_PER_UNIT,A_INVERT_DIR,A_LENGTH,A_MAX_FEED,A_AVG_FEED,A_START_FEED,A_ACCEL_DIST,A_HOME_DIR)
  })
  {};
  Motion(Motion&);
  Motion& operator=(Motion&);

public:
  Axis AXES[NUM_AXES];
  float feedrate;


  float getFeedrate() { return feedrate; }
  void setFeedrate(float f) { feedrate = f; }

  void setStartPosition(MGcode& gcode)
  {
    for(int ax=0;ax<NUM_AXES;ax++)
      gcode.movedata.startpos[ax] = AXES[ax].getCurrentPosition();
  }

  void getMovesteps(MGcode& gcode)
  {
    for(int ax=0;ax < NUM_AXES;ax++)
    {
      gcode.movedata.axismovesteps[ax] = AXES[ax].getMovesteps(gcode.movedata.startpos[ax], gcode[ax].getFloat(), gcode.movedata.axisdirs[ax]);
      if(gcode.movedata.movesteps < gcode.movedata.axismovesteps[ax])
      {
        gcode.movedata.movesteps = gcode.movedata.axismovesteps[ax];
        gcode.movedata.leading_axis = ax;
      }
    }
  }

  unsigned long getLargestStartInterval(MGcode& gcode)
  {
    unsigned long mi = 0;
    for(int ax=0;ax < NUM_AXES;ax++)
    {
      unsigned long t = AXES[ax].getStartInterval(gcode.movedata.feed);
      if(t > mi) mi = t;
    }
    return mi;
  }

  unsigned long getLargestEndInterval(MGcode& gcode)
  {
    unsigned long mi = 0;
    for(int ax=0;ax < NUM_AXES;ax++)
    {
      unsigned long t = AXES[ax].getEndInterval(gcode.movedata.feed);
      if(t > mi) mi = t;
    }
    return mi;
  }

  unsigned long getLargestAccelDistance(MGcode& gcode)
  {
      unsigned long ad = 0;
      for(int ax=0;ax < NUM_AXES;ax++)
      {
        unsigned long t = AXES[ax].getAccelDistance();
        if(t > ad) ad = t;
      }
      return ad;
  }

  void getActualEndpos(MGcode& gcode)
  {
    for(int ax=0;ax<NUM_AXES;ax++)
    {
      gcode.movedata.endpos[ax] = AXES[ax].getEndpos(gcode.movedata.startpos[ax], gcode.movedata.axismovesteps[ax], gcode.movedata.axisdirs[ax]);
    }
  }


  void gcode_precalc(MGcode& gcode, MGcode *prevcode)
  {
    Movedata& md = gcode.movedata;

    if(gcode.state >= MGcode::PREPARED)
      return;

    // We want to carry over the previous ending position and feedrate if possible.
    if(prevcode && prevcode->state >= MGcode::PREPARED)
    {
      md.startpos = prevcode->movedata.endpos;
      if(gcode[F].isUnused())
        md.feed = prevcode->movedata.feed;
    }
    else
    {
      md.feed = feedrate;
      setStartPosition(gcode);
    }

    if(!gcode[F].isUnused())
      md.feed = gcode[F].getFloat();

    if(md.feed == 0)
      md.feed = SAFE_DEFAULT_FEED;


    getMovesteps(gcode);
    md.startinterval = getLargestStartInterval(gcode);
    md.fullinterval = getLargestEndInterval(gcode);
    md.steps_to_accel = getLargestAccelDistance(gcode);
    getActualEndpos(gcode);
    md.accel_until = md.movesteps;
    md.decel_from  = 0;
    md.accel_inc   = 0;

    unsigned long intervaldiff = md.startinterval - md.fullinterval;
    if(intervaldiff > 0)
    {
      if(md.steps_to_accel > md.movesteps / 2)
      {
        md.accel_until = md.movesteps / 2;
        md.decel_from  = md.accel_until - 1;
      }
      else
      {
        md.accel_until = md.movesteps - md.steps_to_accel;
        md.decel_from  = md.steps_to_accel;
      }
      md.accel_inc = intervaldiff / md.steps_to_accel;
    }  

    gcode.state = MGcode::PREPARED;
  }

  void gcode_execute(MGcode& gcode)
  {
    if(gcode.state < MGcode::PREPARED)
      gcode_precalc(gcode, NULL);
    if(gcode.state > MGcode::PREPARED)
      return;

    dumpMovedata(gcode.movedata);
    for(int ax=0;ax<NUM_AXES;ax++)
      AXES[ax].dump_to_host();
    gcode.dump_to_host();

    gcode.state = MGcode::DONE;
  }

  bool axesAreMoving() 
  { 
    for(int ax=0;ax<NUM_AXES;ax++) 
      if(AXES[ax].isMoving()) return true; 
    
    return false; 
  }

  void dumpMovedata(Movedata& md)
  {
    HOST.labelnum("FEED:", md.feed, false);
    HOST.labelnum(" MS:", md.movesteps, false);
    HOST.labelnum(" AMS[0]:", md.axismovesteps[0], false);
    HOST.labelnum(" DIRS[0]:", md.axisdirs[0], false);
    HOST.labelnum(" LEAD:", md.leading_axis, false);
    HOST.labelnum(" SI:", md.startinterval, false);
    HOST.labelnum(" FI:", md.fullinterval, false);
    HOST.labelnum(" STA:", md.steps_to_accel, false);
    HOST.labelnum(" AU:", md.accel_until, false);
    HOST.labelnum(" DF:", md.decel_from, false);
    HOST.labelnum(" AI:", md.accel_inc, false);
    HOST.labelnum(" start[0]:", md.startpos[0], false);
    HOST.labelnum(" end[0]:", md.endpos[0],true);
  }


  void handleInterrupt()
  {
    
  }

  void enableInterrupt() {};
  void disableInterrupt() {};
  void setInterruptCycles(unsigned long cycles) {};

};


extern Motion& MOTION;

#endif // _MOTION_H_