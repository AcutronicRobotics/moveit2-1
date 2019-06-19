/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2013, Unbounded Robotics Inc.
 *  Copyright (c) 2012, Willow Garage, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the Willow Garage nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

/* Author: Michael Ferguson, Ioan Sucan, E. Gil Jones */

#ifndef MOVEIT_PLUGINS_GRIPPER_CONTROLLER_HANDLE
#define MOVEIT_PLUGINS_GRIPPER_CONTROLLER_HANDLE

#include <moveit_simple_controller_manager/action_based_controller_handle.h>
#include <control_msgs/action/gripper_command_action.hpp>
#include <set>

namespace moveit_simple_controller_manager
{
/*
 * This is an interface for a gripper using control_msgs/GripperCommandActionAction
 * action interface (single DOF).
 */
class GripperControllerHandle : public ActionBasedControllerHandle<control_msgs::action::GripperCommandAction>
{
   rclcpp::Logger LOGGER_GRIPPER_CONTROL_HANDLE = rclcpp::get_logger("moveit_simple_controller_manager").get_child("GripperController");
public:
  /* Topics will map to name/ns/goal, name/ns/result, etc */
  GripperControllerHandle(const std::string& name, std::shared_ptr<rclcpp::Node>& node)
    : ActionBasedControllerHandle<control_msgs::action::GripperCommandAction>(name, node)
    , allow_failure_(false)
    , parallel_jaw_gripper_(false)
  {
  }

  bool sendTrajectory(const moveit_msgs::msg::RobotTrajectory& trajectory) override
  {
    RCLCPP_DEBUG(LOGGER_GRIPPER_CONTROL_HANDLE, "Received new trajectory for %s", name_.c_str());

    if (!controller_action_client_)
      return false;

    if (!trajectory.multi_dof_joint_trajectory.points.empty())
    {
      RCLCPP_ERROR(LOGGER_GRIPPER_CONTROL_HANDLE, "Gripper cannot execute multi-dof trajectories.");
      return false;
    }

    if (trajectory.joint_trajectory.points.empty())
    {
      RCLCPP_ERROR(LOGGER_GRIPPER_CONTROL_HANDLE, "GripperController requires at least one joint trajectory point.");
      return false;
    }

    if (trajectory.joint_trajectory.points.size() > 1)
    {
      //TODO (ahcorde)
      RCLCPP_DEBUG(LOGGER_GRIPPER_CONTROL_HANDLE, "Trajectory: ");
      // RCLCPP_DEBUG(LOGGER_GRIPPER_CONTROL_HANDLE, "Trajectory: " << trajectory.joint_trajectory);
    }

    if (trajectory.joint_trajectory.joint_names.empty())
    {
      RCLCPP_ERROR(LOGGER_GRIPPER_CONTROL_HANDLE, "No joint names specified");
      return false;
    }

    std::vector<std::size_t> gripper_joint_indexes;
    for (std::size_t i = 0; i < trajectory.joint_trajectory.joint_names.size(); ++i)
    {
      if (command_joints_.find(trajectory.joint_trajectory.joint_names[i]) != command_joints_.end())
      {
        gripper_joint_indexes.push_back(i);
        if (!parallel_jaw_gripper_)
          break;
      }
    }

    if (gripper_joint_indexes.empty())
    {
      RCLCPP_WARN(LOGGER_GRIPPER_CONTROL_HANDLE, "No command_joint was specified for the MoveIt! controller gripper handle. \
                      Please see GripperControllerHandle::addCommandJoint() and \
                      GripperControllerHandle::setCommandJoint(). Assuming index 0.");
      gripper_joint_indexes.push_back(0);
    }

    // goal to be sent
    control_msgs::action::GripperCommandAction::Goal goal;
    goal.command.position = 0.0;
    goal.command.max_effort = 0.0;

    // send last point
    int tpoint = trajectory.joint_trajectory.points.size() - 1;
    RCLCPP_DEBUG(LOGGER_GRIPPER_CONTROL_HANDLE, "Sending command from trajectory point %d", tpoint);

    // fill in goal from last point
    for (std::size_t i = 0; i < gripper_joint_indexes.size(); ++i)
    {
      std::size_t idx = gripper_joint_indexes[i];

      if (trajectory.joint_trajectory.points[tpoint].positions.size() <= idx)
      {
        RCLCPP_ERROR(LOGGER_GRIPPER_CONTROL_HANDLE, "GripperController expects a joint trajectory with one \
                         point that specifies at least the position of joint \
                         '%s', but insufficient positions provided",
                        trajectory.joint_trajectory.joint_names[idx].c_str());
        return false;
      }
      goal.command.position += trajectory.joint_trajectory.points[tpoint].positions[idx];

      if (trajectory.joint_trajectory.points[tpoint].effort.size() > idx)
        goal.command.max_effort = trajectory.joint_trajectory.points[tpoint].effort[idx];
    }
    //TODO (anasarrak):
    // controller_action_client_->sendGoal(goal,
    //                                     std::bind(&GripperControllerHandle::controllerDoneCallback, this, _1, _2),
    //                                     std::bind(&GripperControllerHandle::controllerActiveCallback, this),
    //                                     std::bind(&GripperControllerHandle::controllerFeedbackCallback, this, _1));

    done_ = false;
    last_exec_ = moveit_controller_manager::ExecutionStatus::RUNNING;
    return true;
  }

  void setCommandJoint(const std::string& name)
  {
    command_joints_.clear();
    addCommandJoint(name);
  }

  void addCommandJoint(const std::string& name)
  {
    command_joints_.insert(name);
  }

  void allowFailure(bool allow)
  {
    allow_failure_ = allow;
  }

  void setParallelJawGripper(const std::string& left, const std::string& right)
  {
    addCommandJoint(left);
    addCommandJoint(right);
    parallel_jaw_gripper_ = true;
  }

private:
  void controllerDoneCallback(const rclcpp_action::ResultCode& state,
                              const std::shared_ptr<const control_msgs::action::GripperCommandAction::Result>& result)
  {
    if (state == rclcpp_action::ResultCode::ABORTED && allow_failure_)
      finishControllerExecution(rclcpp_action::ResultCode::ABORTED);
    else
      finishControllerExecution(state);
  }

  void controllerActiveCallback()
  {
    RCLCPP_DEBUG(LOGGER_GRIPPER_CONTROL_HANDLE, "%s started execution",name_.c_str());
  }

  void controllerFeedbackCallback(const std::shared_ptr<const control_msgs::action::GripperCommandAction::Feedback>& feedback)
  {
  }

  /*
   * Some gripper drivers may indicate a failure if they do not close all the way when
   * an object is in the gripper.
   */
  bool allow_failure_;

  /*
   * A common setup is where there are two joints that each move
   * half the overall distance. Thus, the command to the gripper
   * should be the sum of the two joint distances.
   *
   * When this is set, command_joints_ should be of size 2,
   * and the command will be the sum of the two joints.
   */
  bool parallel_jaw_gripper_;

  /*
   * A GripperCommandAction message has only a single float64 for the
   * "command", thus only a single joint angle can be sent -- however,
   * due to the complexity of making grippers look correct in a URDF,
   * they typically have >1 joints. The "command_joint" is the joint
   * whose position value will be sent in the GripperCommandAction action. A
   * set of names is provided for acceptable joint names. If any of
   * the joints specified is found, the value corresponding to that
   * joint is considered the command.
   */
  std::set<std::string> command_joints_;
};

}  // end namespace moveit_simple_controller_manager

#endif  // MOVEIT_PLUGINS_GRIPPER_CONTROLLER_HANDLE
