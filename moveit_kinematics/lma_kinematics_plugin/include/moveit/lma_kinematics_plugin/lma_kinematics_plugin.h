/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2016, CRI group, NTU, Singapore
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
 *   * Neither the name of CRI group nor the names of its
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

/* Author: Francisco Suarez-Ruiz */

#ifndef MOVEIT_ROS_PLANNING_LMA_KINEMATICS_PLUGIN_H
#define MOVEIT_ROS_PLANNING_LMA_KINEMATICS_PLUGIN_H

// ROS2
#include "rclcpp/rclcpp.hpp"
#include <random_numbers/random_numbers.h>

// ROS msgs
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <moveit_msgs/srv/get_position_fk.hpp>
#include <moveit_msgs/srv/get_position_ik.hpp>
#include <moveit_msgs/msg/kinematic_solver_info.hpp>
#include <moveit_msgs/msg/move_it_error_codes.hpp>

// KDL
#include <kdl/config.h>
#include <kdl/chainfksolver.hpp>
#include <kdl/chainiksolver.hpp>

// MoveIt!
#include <moveit/kinematics_base/kinematics_base.h>
#include <moveit/robot_model/robot_model.h>
#include <moveit/robot_state/robot_state.h>

namespace lma_kinematics_plugin
{
/**
 * @brief Implementation of kinematics using Levenberg-Marquardt (LMA) solver from KDL.
 * This version supports any kinematic chain without mimic joints.
 */
class LMAKinematicsPlugin : public kinematics::KinematicsBase
{
public:
  /**
   *  @brief Default constructor
   */
  LMAKinematicsPlugin();

  bool getPositionIK(
      const geometry_msgs::msg::Pose& ik_pose, const std::vector<double>& ik_seed_state, std::vector<double>& solution,
      moveit_msgs::msg::MoveItErrorCodes& error_code,
      const kinematics::KinematicsQueryOptions& options = kinematics::KinematicsQueryOptions()) const override;

  bool searchPositionIK(
      const geometry_msgs::msg::Pose& ik_pose, const std::vector<double>& ik_seed_state, double timeout,
      std::vector<double>& solution, moveit_msgs::msg::MoveItErrorCodes& error_code,
      const kinematics::KinematicsQueryOptions& options = kinematics::KinematicsQueryOptions()) const override;

  bool searchPositionIK(
      const geometry_msgs::msg::Pose& ik_pose, const std::vector<double>& ik_seed_state, double timeout,
      const std::vector<double>& consistency_limits, std::vector<double>& solution,
      moveit_msgs::msg::MoveItErrorCodes& error_code,
      const kinematics::KinematicsQueryOptions& options = kinematics::KinematicsQueryOptions()) const override;

  bool searchPositionIK(
      const geometry_msgs::msg::Pose& ik_pose, const std::vector<double>& ik_seed_state, double timeout,
      std::vector<double>& solution, const IKCallbackFn& solution_callback, moveit_msgs::msg::MoveItErrorCodes& error_code,
      const kinematics::KinematicsQueryOptions& options = kinematics::KinematicsQueryOptions()) const override;

  bool searchPositionIK(
      const geometry_msgs::msg::Pose& ik_pose, const std::vector<double>& ik_seed_state, double timeout,
      const std::vector<double>& consistency_limits, std::vector<double>& solution,
      const IKCallbackFn& solution_callback, moveit_msgs::msg::MoveItErrorCodes& error_code,
      const kinematics::KinematicsQueryOptions& options = kinematics::KinematicsQueryOptions()) const override;

  bool getPositionFK(const std::vector<std::string>& link_names, const std::vector<double>& joint_angles,
                     std::vector<geometry_msgs::msg::Pose>& poses) const override;

  bool initialize(const moveit::core::RobotModel& robot_model, const std::string& group_name,
                  const std::string& base_frame, const std::vector<std::string>& tip_frames,
                  double search_discretization) override;

  /**
   * @brief  Return all the joint names in the order they are used internally
   */
  const std::vector<std::string>& getJointNames() const override;

  /**
   * @brief  Return all the link names in the order they are represented internally
   */
  const std::vector<std::string>& getLinkNames() const override;

protected:
  /**
   * @brief Given a desired pose of the end-effector, search for the joint angles required to reach it.
   * This particular method is intended for "searching" for a solutions by randomly re-seeding on failure.
   * @param ik_pose the desired pose of the link
   * @param ik_seed_state an initial guess solution for the inverse kinematics
   * @param timeout The amount of time (in seconds) available to the solver
   * @param solution the solution vector
   * @param solution_callback A callback to validate an IK solution
   * @param error_code an error code that encodes the reason for failure or success
   * @param consistency_limits The returned solutuion will not deviate more than these from the seed
   * @return True if a valid solution was found, false otherwise
   */
  bool searchPositionIK(const geometry_msgs::msg::Pose& ik_pose, const std::vector<double>& ik_seed_state, double timeout,
                        std::vector<double>& solution, const IKCallbackFn& solution_callback,
                        moveit_msgs::msg::MoveItErrorCodes& error_code, const std::vector<double>& consistency_limits,
                        const kinematics::KinematicsQueryOptions& options = kinematics::KinematicsQueryOptions()) const;

private:
  bool timedOut(const std::chrono::system_clock::time_point& start_time, double duration) const;

  /** @brief Check whether the solution lies within the consistency limits of the seed state
   *  @param seed_state Seed state
   *  @param consistency_limits
   *  @param solution solution configuration
   *  @return true if check succeeds
   */
  bool checkConsistency(const Eigen::VectorXd& seed_state, const std::vector<double>& consistency_limits,
                        const Eigen::VectorXd& solution) const;
  /** Check whether joint values satisfy joint limits */
  bool obeysLimits(const Eigen::VectorXd& values) const;
  /** Harmonize revolute joint values into the range -2 Pi .. 2 Pi */
  void harmonize(Eigen::VectorXd& values) const;

  void getRandomConfiguration(Eigen::VectorXd& jnt_array) const;

  /** @brief Get a random configuration within consistency limits close to the seed state
   *  @param seed_state Seed state
   *  @param consistency_limits
   *  @param jnt_array Returned random configuration
   */
  void getRandomConfiguration(const Eigen::VectorXd& seed_state, const std::vector<double>& consistency_limits,
                              Eigen::VectorXd& jnt_array) const;

  bool initialized_;  ///< Internal variable that indicates whether solver is configured and ready

  unsigned int dimension_;                        ///< Dimension of the group
  moveit_msgs::msg::KinematicSolverInfo solver_info_;  ///< Stores information for the inverse kinematics solver

  const robot_model::JointModelGroup* joint_model_group_;
  robot_state::RobotStatePtr state_;
  KDL::Chain kdl_chain_;
  std::unique_ptr<KDL::ChainFkSolverPos> fk_solver_;
  std::vector<const robot_model::JointModel*> joints_;
  std::vector<std::string> joint_names_;
  rclcpp::Node::SharedPtr node_;

  int max_solver_iterations_;
  double epsilon_;
  /** weight of orientation error vs position error
   *
   * < 1.0: orientation has less importance than position
   * > 1.0: orientation has more importance than position
   * = 0.0: perform position-only IK */
  double orientation_vs_position_weight_;
};
}

#endif
