/**
 * Created by Nikos Siatras
 * Twitter: nsiatras
 * Website: https://www.sourcerabbit.com
 */

#include "Backlash.h"

#define DIR_POSITIVE 0
#define DIR_NEGATIVE 1
#define DIR_NEUTRAL 2

static float previous_targets[MAX_N_AXIS] = {0.000};
static uint8_t axis_directions[MAX_N_AXIS] = {DIR_NEUTRAL};

// This array holds the amount of millimeters that has been added to each axes
// in order to remove backlash.
// The system_convert_axis_steps_to_mpos (located in System.cpp) uses this array to remove the backlash added
// in order to keep the absolute machine position (position without any backlash distance added)
float backlash_compensation_to_remove_from_mpos[MAX_N_AXIS];

void backlash_ini()
{
    // The backlash_ini method is called from Grbl.cpp
    for (int i = 0; i < MAX_N_AXIS; i++)
    {
        previous_targets[i] = 0.0;
        backlash_compensation_to_remove_from_mpos[i] = 0.0;
        axis_directions[i] = DIR_NEUTRAL;
    }
}

/**
 * Plans and queues a backlash motion into planner buffer
 */
void backlash_compensate_backlash(float *target, plan_line_data_t *pl_data)
{
    /*char stringArray[10];
      sprintf(stringArray, "%f", previous_targets[2]);
      grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, stringArray);*/

    float backlash_compensation_target[MAX_N_AXIS] = {0.0};
    bool perform_backlash_compensation_motion = false;

    for (int axis = 0; axis < MAX_N_AXIS; axis++)
    {
        backlash_compensation_target[axis] = previous_targets[axis];

        if (axis_settings[axis]->backlash->get() > 0)
        {
            if (target[axis] > previous_targets[axis])
            {
                // The new axis target is "Positive" compared to the previous one.
                // If the last axis target was "Negative" then alter the backlash_compensation_target for this axis.
                if (axis_directions[axis] == DIR_NEGATIVE)
                {
                    backlash_compensation_target[axis] += axis_settings[axis]->backlash->get();
                    perform_backlash_compensation_motion = true;
                    backlash_compensation_to_remove_from_mpos[axis] += axis_settings[axis]->backlash->get();
                }

                axis_directions[axis] = DIR_POSITIVE;
            }
            else if (target[axis] < previous_targets[axis])
            {
                // The new axis target is "Negative" compared to the previous one.
                // If the last axis target was "Positive" then alter the backlash_compensation_target for this axis.
                if (axis_directions[axis] == DIR_POSITIVE)
                {
                    backlash_compensation_target[axis] -= axis_settings[axis]->backlash->get();
                    perform_backlash_compensation_motion = true;
                    backlash_compensation_to_remove_from_mpos[axis] -= axis_settings[axis]->backlash->get();
                }

                axis_directions[axis] = DIR_NEGATIVE;
            }
        }

        previous_targets[axis] = target[axis];
    }

    if (perform_backlash_compensation_motion)
    {
        // grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "Anti backlash Motion");

        // Queue the backlash motion into planner buffer
        plan_line_data_t pl_backlash_data;
        plan_line_data_t *backlash_data = &pl_backlash_data;
        memset(backlash_data, 0, sizeof(plan_line_data_t)); // Zero backlash_data struct

        backlash_data->spindle = pl_data->spindle;
        backlash_data->spindle_speed = pl_data->spindle_speed;
        backlash_data->feed_rate = pl_data->feed_rate;
        backlash_data->coolant = pl_data->coolant;
        backlash_data->motion = {};
        backlash_data->motion.antiBacklashMotion = 1;

        do
        {
            protocol_execute_realtime(); // Check for any run-time commands
            if (sys.abort)
            {
                return; // Bail, if system abort.
            }

            if (plan_check_full_buffer())
            {
                protocol_auto_cycle_start(); // Auto-cycle start when buffer is full.
            }
            else
            {
                break;
            }
        } while (1);

        // Plan and queue the backlash motion into planner buffer
        plan_buffer_line(backlash_compensation_target, backlash_data);
    }
}

/**
 * The backlash_reset_targets is been called from limits_go_home
 * */
void backlash_reset_targets(float target[])
{
    for (int i = 0; i < MAX_N_AXIS; i++)
    {
        previous_targets[i] = target[i];
        axis_directions[i] = DIR_NEUTRAL;
    }
}

void backlash_synch_position()
{
    // Update target_prev
    system_convert_array_steps_to_mpos(previous_targets, sys_position);
    for (int i = 0; i < MAX_N_AXIS; i++)
    {
        previous_targets[i] = previous_targets[i] - backlash_compensation_to_remove_from_mpos[i];
    }
}