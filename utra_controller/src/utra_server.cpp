/* Copyright 2021 Umbratek Inc. All Rights Reserved.
 *
 * Software License Agreement (BSD License)
 *
 * Author: johnson <johnson@umbratek.com>
 ============================================================================*/
#include "ros/ros.h"
#include "utra/utra_api_tcp.h"
#include "utra/utra_flxie_api.h"

#include "utra_msg/Connect.h"
#include "utra_msg/Disconnect.h"
#include "utra_msg/Checkconnect.h"
#include "utra_msg/Mservojoint.h"
#include "utra_msg/Grippermv.h"
#include "utra_msg/EnableSet.h"
#include "utra_msg/GripperStateGet.h"
#include "utra_msg/GripperStateSet.h"
#include "utra_msg/GetInt16.h"
#include "utra_msg/SetInt16.h"
#include "utra_msg/GetFloat32.h"
#include "utra_msg/SetFloat32.h"
#include "utra_msg/GetUInt16A.h"
#include "utra_msg/GetFloat32A.h"

#include "utra_msg/MovetoJointP2p.h"
#include "utra_msg/MovetoCartesianLine.h"
#include "utra_msg/MovetoCartesianLineB.h"

UtraApiTcp *utra = NULL;
UtraFlxiE2Api *fixi = NULL;


std::string utra_ip = "";

constexpr unsigned int hash(const char *s, int off = 0) { return !s[off] ? 5381 : (hash(s, off + 1) * 33) ^ s[off]; }


bool connect_api(utra_msg::Connect::Request &req, utra_msg::Connect::Response &res) {
  if (utra != NULL) {
    res.ret=0;
    res.message="server have connected utra";
    return true;
  }
  utra_ip = req.ip_address;
  char *ip = new char[req.ip_address.length() + 1];
  std::strcpy(ip, req.ip_address.c_str());
  utra = new UtraApiTcp(ip);
  uint8_t axis;
  int ret = utra->get_axis(&axis);
  res.ret=ret;
  if (ret == -3) {
    res.message="can not connect the utra";
  } else {
    res.message="connect seccess";
  }
  return true;
}

bool disconnect_api(utra_msg::Disconnect::Request &req, utra_msg::Disconnect::Response &res) {
  if(utra == NULL){
    res.ret=0;
    res.message="server have not connected utra";
    return true;
  }
  res.ret=0;
  res.message="utra have disconnected";
  delete utra;
  utra = NULL;
  return true;
}

bool check_c_api(utra_msg::Checkconnect::Request &req, utra_msg::Checkconnect::Response &res) {
  if(utra == NULL){
    res.ret=-3;
    res.ip_address = "";
    res.message="server have not connected utra";
    return true;
  }
  uint8_t axis;
  int ret = utra->get_axis(&axis);
  res.ip_address = utra_ip;
  res.ret=ret;
  if (ret == -3) {
    res.message="can not connect the utra";
  } else {
    res.message="connect seccess";
  }
  return true;
}


void print_joints(float* frames,int con)
{
  for (size_t j = 0; j < con; j++)
  { 
    ROS_INFO("%f %f %f %f %f %f",frames[j*6],frames[j*6+1],frames[j*6+2],frames[j*6+3],frames[j*6+4],frames[j*6+5]);
  }
}

bool mv_servo_joint(utra_msg::Mservojoint::Request &req, utra_msg::Mservojoint::Response &res) {
  if(utra == NULL){
    res.ret=-3;
    res.message="server have not connected utra";
    return true;
  }
  int RET_ERROR_COUNT = 0; //发送错误计数
  int axiz = req.axiz;
  int CON = 3; //how many points with one time to send
  if(axiz == 6){
    int frames_num = req.num;
    ROS_INFO("mv_servo_joint %d %f",frames_num,req.time);
    
    float frames[CON*6] = {0};
    float mvtime[CON] = {0};
    for (size_t i = 0; i < CON; i++)
    {
      mvtime[i] = req.time;
    }
    

    int have_CON_s = frames_num/CON;
    int have_CON_m = frames_num%CON;

    int req_frames_index = 0;
    int ret = 0;
  //    for (size_t j = 0; j < frames_num; j++)
  // { 
  //   ROS_INFO("%f %f %f %f %f %f",req.frames[j*6],req.frames[j*6+1],req.frames[j*6+2],req.frames[j*6+3],frames[j*6+4],req.frames[j*6+5]);
  // }
    //for run have_CON_s time send
    if(req.plan_delay>0){
      ROS_INFO("plan_sleep %f",req.plan_delay);
      utra->plan_sleep(req.plan_delay); // set utra sleep 0.2s to wait for command 
    }
    for (size_t i = 0; i < have_CON_s; i++)
    {
      for (size_t j = 0; j < CON*6; j++)
      { 
        frames[j] = req.frames[req_frames_index+j];
      }
      // print_joints(frames,CON);
      req_frames_index = req_frames_index + CON*6;
      ret = utra->moveto_servo_joint(CON, frames, mvtime);
      ROS_INFO("req_frames_index %d ret %d",req_frames_index,ret);
      if(ret != 0 ){
        RET_ERROR_COUNT++;
      }else{
        if(RET_ERROR_COUNT>0){
          RET_ERROR_COUNT--;
        }
      }
      if(RET_ERROR_COUNT>3){
        res.ret=-3;
        res.message="RET_ERROR_COUNT more than 3 times";
        ROS_ERROR("RET_ERROR_COUNT more than 3 times");
        return true;
      }
    }
    
    //last time send
    for (size_t i = 0; i < have_CON_m*6; i++)
    { 
      frames[i] = req.frames[req_frames_index+i];
    }
    // print_joints(frames,have_CON_m);
    ret = utra->moveto_servo_joint(have_CON_m, frames, mvtime);

    ROS_INFO("ret %d",ret);
    res.ret=ret;
    res.message="send cmd success";
    return true;
  }
  return true;
}


bool gripper_state_set(utra_msg::GripperStateSet::Request &req, utra_msg::GripperStateSet::Response &res) {
  ROS_INFO("state value: %d",req.state);
  if(utra == NULL){
    res.ret=-3;
    res.message="server have not connected utra";
    return true;
  }
  if(req.state == 1){
    if(fixi == NULL){
      fixi = new UtraFlxiE2Api(utra, 101);
    }
    int ret = fixi->set_motion_mode(1);
    ROS_INFO("set_motion_mode: %d\n", ret);
    ret = fixi->set_motion_enable(1);
    printf("set_motion_enable: %d\n", ret);
    float value;
    ret = fixi->get_pos_target(&value);
    res.pos = value;
    res.message="OK";
    return true;
  }
  if(req.state == 0){
    if(fixi == NULL){
      res.ret=0;
      res.message="server have not connected gripper";
      return true;
    }else{
      int ret = fixi->set_motion_mode(0);
      ROS_INFO("set_motion_mode: %d\n", ret);
      ret = fixi->set_motion_enable(0);
      printf("set_motion_enable: %d\n", ret);
      res.ret=ret;
      res.message="OK";
      return true;
    }
  }
  return true;
}
bool gripper_state_get(utra_msg::GripperStateGet::Request &req, utra_msg::GripperStateGet::Response &res) {
    if(fixi == NULL){
      res.ret=-3;
      res.enable=0;
      res.message="server have not connected gripper";
      return true;
    }else{
      float value;
      int ret = fixi->get_pos_target(&value);
      if(ret != -3){
        ros::NodeHandle n;
        float pos = (value/100)-0.4;
        n.setParam("utra_gripper_pos",pos);
      }
      uint8_t enable;
      ret = fixi->get_motion_enable(&enable);
      res.ret=ret;
      res.enable=enable;
      res.pos=value;
      res.message="OK";
      return true;
    }
  
  return true;
}
bool gripper_mv(utra_msg::Grippermv::Request &req, utra_msg::Grippermv::Response &res) {
  if(utra == NULL || fixi == NULL){
    res.ret=-3;
    res.message="server have not connected utra or gripper";
    return true;
  }
  int ret = fixi->set_pos_target(req.pos);
  if(ret != -3){
    ros::NodeHandle n;
    float pos = (req.pos/100)-0.4;
    n.setParam("utra_gripper_pos",pos);
  }
  res.ret=ret;
  res.message="OK";
  return true;
}

bool status_set(utra_msg::SetInt16::Request &req, utra_msg::SetInt16::Response &res) {
  if(utra == NULL)
  {
    res.ret=-3;
  }
  int ret = utra->set_motion_status(req.data);
  res.ret=ret;
  return true;
}
bool status_get(utra_msg::GetInt16::Request &req, utra_msg::GetInt16::Response &res) {
  if(utra == NULL)
  {
    res.ret=-3;
  }
  
  uint8_t status;
  int ret = utra->get_motion_status(&status);
  res.ret=ret;
  res.data = status;
  ROS_INFO("status_get ret %d, status %d",ret,status);
  return true;
}
bool mode_set(utra_msg::SetInt16::Request &req, utra_msg::SetInt16::Response &res) {
  if(utra == NULL)
  {
    res.ret=-3;
  }
  int ret = utra->set_motion_mode(req.data);
  res.ret=ret;
  return true;
}
bool mode_get(utra_msg::GetInt16::Request &req, utra_msg::GetInt16::Response &res) {
  if(utra == NULL)
  {
    res.ret=-3;
  }
  uint8_t mode;
  int ret = utra->get_motion_mode(&mode);
  res.ret=ret;
  res.data = mode;
  return true;
}
bool enable_set(utra_msg::EnableSet::Request &req, utra_msg::EnableSet::Response &res) {
  if(utra == NULL)
  {
    res.ret=-3;
  }
  int ret = utra->set_motion_enable(req.axis,req.enable);
  res.ret=ret;
  return true;
}
bool enable_get(utra_msg::GetInt16::Request &req, utra_msg::GetInt16::Response &res) {
  if(utra == NULL)
  {
    res.ret=-3;
  }
  int enable;
  int ret = utra->get_motion_enable(&enable);
  res.ret=ret;
  res.data = enable;
  return true;
}
bool gripper_vel_set(utra_msg::SetFloat32::Request &req, utra_msg::SetFloat32::Response &res) {
  if(utra == NULL || fixi == NULL){
    res.ret=-3;
    return true;
  }
  int ret1 = fixi->set_vel_limit_min(-req.data,true);
  int ret2 = fixi->set_vel_limit_max(req.data,true);
  res.ret=ret1+ret2;
  return true;
}
bool gripper_vel_get(utra_msg::GetFloat32::Request &req, utra_msg::GetFloat32::Response &res) {
  if(utra == NULL || fixi == NULL){
    res.ret=-3;
    return true;
  }
  float value;
  int ret1 = fixi->get_vel_limit_min(&value);
  int ret2 = fixi->get_vel_limit_max(&value);
  res.ret=ret1+ret2;
  res.data = value;
  return true;
}

bool gripper_acc_set(utra_msg::SetFloat32::Request &req, utra_msg::SetFloat32::Response &res) {
  if(utra == NULL || fixi == NULL){
    res.ret=-3;
    return true;
  }
  float data = req.data;
  if(req.data<0){
    res.ret=-3;
    return true;
  }
  if(req.data>300){
    data = 300;
  }
  int ret = fixi->set_pos_adrc_param(3,data);
  res.ret=ret;
  return true;
}
bool gripper_acc_get(utra_msg::GetFloat32::Request &req, utra_msg::GetFloat32::Response &res) {
  if(utra == NULL || fixi == NULL){
    res.ret=-3;
    return true;
  }
  float value;
  int ret = fixi->get_pos_adrc_param(3,&value);
  res.ret=ret;
  res.data = value;
  return true;
}
bool gripper_error_code(utra_msg::GetInt16::Request &req, utra_msg::GetInt16::Response &res) {
  if(utra == NULL || fixi == NULL){
    res.ret=-3;
    return true;
  }
  uint8_t value;
  int ret = fixi->get_error_code(&value);
  res.ret=ret;
  res.data = value;
  return true;
}

bool gripper_reset_err(utra_msg::GetInt16::Request &req, utra_msg::GetInt16::Response &res) {
  if(utra == NULL || fixi == NULL){
    res.ret=-3;
    return true;
  }
  int ret = fixi->reset_err();
  res.ret=ret;
  res.data = 0;
  return true;
}

bool gripper_unlock(utra_msg::SetInt16::Request &req, utra_msg::SetInt16::Response &res) {
  if(utra == NULL || fixi == NULL){
    res.ret=-3;
    return true;
  }
  int data = req.data;
  int ret = fixi->set_unlock_function(data);
  res.ret=ret;
  return true;
}

bool get_error_code(utra_msg::GetUInt16A::Request &req, utra_msg::GetUInt16A::Response &res) {
  if(utra == NULL){
    res.ret=-3;
    return true;
  }
  uint8_t array[24] = {0};
  int ret = utra->get_error_code(array);
  res.ret=ret;
  for (size_t j = 0; j < 24; j++)
    {
      res.data.push_back(array[j]);
    }
  return true;
}
bool get_servo_msg(utra_msg::GetUInt16A::Request &req, utra_msg::GetUInt16A::Response &res) {
  if(utra == NULL){
    res.ret=-3;
    return true;
  }
  uint8_t array[24] = {0};
  int ret = utra->get_servo_msg(array);
  res.ret=ret;
  for (size_t j = 0; j < 24; j++)
    {
      res.data.push_back(array[j]);
    }
  return true;
}
bool moveto_joint_p2p(utra_msg::MovetoJointP2p::Request &req, utra_msg::MovetoJointP2p::Response &res) {
  if(utra == NULL){
    res.ret=-3;
    return true;
  }
  int axis = req.joints.size();
  if(axis == 6){
    float joints[6] = {req.joints[0],req.joints[1],req.joints[2],req.joints[3],req.joints[4],req.joints[5]};
    int ret = utra->moveto_joint_p2p(joints, req.speed, req.acc, 0);
    res.ret = ret;
    return true;
  }else{
    res.ret = -3;
    return true;
  }
}

bool moveto_cartesian_line(utra_msg::MovetoCartesianLine::Request &req, utra_msg::MovetoCartesianLine::Response &res) {
  if(utra == NULL){
    res.ret=-3;
    return true;
  }
  int axis = req.pose.size();
  if(axis == 6){
    float pose[6] = {req.pose[0],req.pose[1],req.pose[2],req.pose[3],req.pose[4],req.pose[5]};
    int ret = utra->moveto_cartesian_line(pose, req.speed, req.acc, 0);
    res.ret = ret;
    return true;
  }else{
    res.ret = -3;
    return true;
  }
}

bool plan_sleep(utra_msg::SetFloat32::Request &req, utra_msg::SetFloat32::Response &res) {
  if(utra == NULL){
    res.ret=-3;
    return true;
  }
  int ret = utra->plan_sleep(req.data);
  res.ret=ret;
  return true;
}
bool moveto_cartesian_lineb(utra_msg::MovetoCartesianLineB::Request &req, utra_msg::MovetoCartesianLineB::Response &res) {
  if(utra == NULL){
    res.ret=-3;
    return true;
  }
  int axis = req.pose.size();
  if(axis == 6){
    float pose[6] = {req.pose[0],req.pose[1],req.pose[2],req.pose[3],req.pose[4],req.pose[5]};
    int ret = utra->moveto_cartesian_lineb(pose, req.speed, req.acc, 0, req.radii);
    res.ret = ret;
    return true;
  }else{
    res.ret = -3;
    return true;
  }
}

bool get_joint_actual_pos(utra_msg::GetFloat32A::Request &req, utra_msg::GetFloat32A::Response &res) {
  if(utra == NULL){
    res.ret=-3;
    return true;
  }
  float array[10] = {0};
  int ret = utra->get_joint_target_pos(array);
  res.ret=ret;
  for (size_t j = 0; j < 10; j++)
    {
      res.data.push_back(array[j]);
    }
  return true;
}


int main(int argc, char **argv) {
  ros::init(argc, argv, "utra_server");
  ros::NodeHandle nh;
  
  if (nh.getParam("utra_ip", utra_ip)) {
    ROS_INFO("Got param: %s", utra_ip.c_str());
  } else {
    ROS_ERROR("Failed to get param 'utra_ip'");
    utra_ip="";
    // return -1;
  }
  if(utra_ip != ""){
    char *ip = new char[utra_ip.length() + 1];
    std::strcpy(ip, utra_ip.c_str());
    utra = new UtraApiTcp(ip);
    uint8_t axis;
    int ret = utra->get_axis(&axis);
    ROS_INFO("ret %d", ret);
    if (ret == -3) {
      ROS_ERROR("can not connect the utra");
      // return -1;
    }
  }

  ros::ServiceServer connect = nh.advertiseService("utra/connect", connect_api);
  ros::ServiceServer disconnect = nh.advertiseService("utra/disconnect", disconnect_api);
  ros::ServiceServer check_c = nh.advertiseService("utra/check_connect", check_c_api);
  ros::ServiceServer mservojoint = nh.advertiseService("utra/mv_servo_joint", mv_servo_joint);
  
  ros::ServiceServer statusset = nh.advertiseService("utra/status_set", status_set);
  ros::ServiceServer statusget = nh.advertiseService("utra/status_get", status_get);
  ros::ServiceServer modeset = nh.advertiseService("utra/mode_set", mode_set);
  ros::ServiceServer modeget = nh.advertiseService("utra/mode_get", mode_get);
  ros::ServiceServer enableset = nh.advertiseService("utra/enable_set", enable_set);
  ros::ServiceServer enableget = nh.advertiseService("utra/enable_get", enable_get);

  ros::ServiceServer grippermv = nh.advertiseService("utra/gripper_mv", gripper_mv);
  ros::ServiceServer gripperstateset = nh.advertiseService("utra/gripper_state_set", gripper_state_set);
  ros::ServiceServer gripperstateget = nh.advertiseService("utra/gripper_state_get", gripper_state_get);
  ros::ServiceServer grippervelset = nh.advertiseService("utra/gripper_vel_set", gripper_vel_set);
  ros::ServiceServer grippervelget = nh.advertiseService("utra/gripper_vel_get", gripper_vel_get);
  ros::ServiceServer gripperaccset = nh.advertiseService("utra/gripper_acc_set", gripper_acc_set);
  ros::ServiceServer gripperaccget = nh.advertiseService("utra/gripper_acc_get", gripper_acc_get);
  ros::ServiceServer gripperreseterr = nh.advertiseService("utra/gripper_reset_err", gripper_reset_err);
  ros::ServiceServer grippererrorcode = nh.advertiseService("utra/gripper_error_code", gripper_error_code);
  ros::ServiceServer gripperunlock = nh.advertiseService("utra/gripper_unlock", gripper_unlock);
  

  ros::ServiceServer geterrorcode = nh.advertiseService("utra/get_error_code", get_error_code);
  ros::ServiceServer getservomsg = nh.advertiseService("utra/get_servo_msg", get_servo_msg);
  
  ros::ServiceServer movetojointp2p = nh.advertiseService("utra/moveto_joint_p2p", moveto_joint_p2p);
  ros::ServiceServer movetocartesianline = nh.advertiseService("utra/moveto_cartesian_line", moveto_cartesian_line);
  ros::ServiceServer plansleep = nh.advertiseService("utra/plan_sleep", plan_sleep);
  ros::ServiceServer movetocartesianlineb = nh.advertiseService("utra/moveto_cartesian_lineb", moveto_cartesian_lineb);
  // ros::ServiceServer moveto_cartesian_circle = nh.advertiseService("utra/moveto_cartesian_circle", moveto_cartesian_circle);
  ros::ServiceServer getjointactualpos = nh.advertiseService("utra/get_joint_actual_pos", get_joint_actual_pos);

  ROS_INFO("Ready for utra server.");
  ros::spin();
}