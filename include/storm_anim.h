#ifndef STORM_ANIM_H
#define STORM_ANIM_H

typedef struct {
    float r_arm_forward, l_arm_forward;
    float r_arm_lift, l_arm_lift;
    float r_leg_forward, l_leg_forward;
    float r_leg_lift, l_leg_lift;
} StormPose;

/* Evaluate a procedural pose from the current combat state. Timer units are frames. */
void storm_anim_evaluate(StormPose *pose, int attacking, int combo,
                        int timer, int combo_direction, float gait_phase);

#endif
