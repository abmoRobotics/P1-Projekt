#include <ros/ros.h>
#include <main_pkg/poseArray_srv.h>
#include <geometry_msgs/PoseArray.h>
#include <geometry_msgs/PointStamped.h>
#include <move_base_msgs/MoveBaseGoal.h>
#include <move_base_msgs/MoveBaseAction.h>
#include <actionlib/client/simple_action_client.h>
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>
#include <kobuki_msgs/ButtonEvent.h>
#include <kobuki_msgs/SensorState.h>
//#include <rate.h>

void debug(std::string a)
{
    enum mode
    {
        debug,
        operate,
    };
    mode mode = debug;

    if (mode == debug)
    {
        std::cout << "Test: " << a << std::endl;
    }
}

class MoveBase
{
private:

    enum navMode
    {
        automatic,
        operation
    };
    navMode _navMode = automatic;
    
    //nodeHandle defined
    ros::NodeHandle nh;
    //Actions are defined
    actionlib::SimpleActionClient<move_base_msgs::MoveBaseAction> MoveBaseClient;
    //Services are defined
    ros::ServiceClient _client_receive_pose_array = nh.serviceClient<main_pkg::poseArray_srv>("get_job");
    ros::ServiceClient _client_receive_pose_kitchen = nh.serviceClient<main_pkg::pointStamped_srv>("get_pose_kitchen");
    //Subscribers
    ros::Subscriber sub = nh.subscribe("/mobile_base/events/button", 0, &MoveBase::_send_task,
 this);
    ros::Subscriber batterySub = nh.subscribe("/mobile_base/sensors/core", 1, &MoveBase::batteryCallback, this);
    //Service message for current job
    main_pkg::poseArray_srv _srv_receive_pose_array;
    main_pkg::pointStamped_srv _srv_receive_pose_kitchen;

    //Publisher for markers
    ros::Publisher pub_marker = nh.advertise<visualization_msgs::MarkerArray>(
        "route_markers", 1);
    //Variables
    int length_job = 0;
    int minimum_battery_pct = 95; //Battery % when it returns to dock
    int kobuki_max_charge = 163; //Battery volt at full charge
    int current_battery = kobuki_max_charge; //Current battery in volt
    int current_dock_state = 2;

public:
    int job_size()
    {
        return _srv_receive_pose_array.response.arr.poses.size();
    }

    void _receive_pose_array()
    {
        debug("1");
        _client_receive_pose_array.call(_srv_receive_pose_array);
        
        debug("2");
        if (!_srv_receive_pose_array.response.arr.poses.empty())
        {
            length_job = _srv_receive_pose_array.response.arr.poses.size();
            _send_markers(_srv_receive_pose_array.response);
            debug("3");
            //_send_goal(_srv_receive_pose_array.response);
            debug("4");
        }
        else
        {
            _delete_markers();
            std::cout << "No jobs debugnding" << std::endl;
        }
        
    }

   //Button callback
    void _send_task(const kobuki_msgs::ButtonEvent::ConstPtr &msg)
    {
        debug("hejhejhej");
        if (_navMode == this->automatic)
        {
            debug("Auto");
            for (int i = 0; i < length_job; i++)
            {
                _send_goal(_srv_receive_pose_array.response);
            }
        }
        else if (_navMode == this->operation)
        {
            debug("operation");
            if (!_srv_receive_pose_array.response.arr.poses.empty())
            {
                /* code */
            }
            else
            {
            }

            _send_goal(_srv_receive_pose_array.response);
        }
    }  

    int _send_goal(main_pkg::poseArray_srv::Response pose_array)
    {
        move_base_msgs::MoveBaseGoal goal;
        debug("5");
        goal.target_pose.pose = pose_array.arr.poses[0];
        debug("6");
        goal.target_pose.header.frame_id = pose_array.arr.header.frame_id;
        debug("7");
        goal.target_pose.header.stamp = ros::Time::now();
        debug("8");
        MoveBaseClient.waitForServer();
        debug("9");
        MoveBaseClient.sendGoal(goal);
        debug("10");
        MoveBaseClient.waitForResult();
        debug("11");
        if (MoveBaseClient.getState() == actionlib::SimpleClientGoalState::SUCCEEDED)
        {
            std::cout << "Reached goal: " << 1 + 1 << " OF " << length_job << std::endl;
        }
        else
        {
            std::cout << "ERROR - Current State: " << MoveBaseClient.getState().toString() << std::endl;
        }

        _srv_receive_pose_array.response.arr.poses.erase(_srv_receive_pose_array.response.arr.poses.begin());
    }


    void _delete_markers()
    {
        visualization_msgs::Marker marker;
        visualization_msgs::MarkerArray marker_array;
        marker.header.stamp = ros::Time::now();
        marker.action = visualization_msgs::Marker::DELETEALL;
        marker_array.markers.push_back(marker);
        pub_marker.publish(marker_array);
    }

    void _send_markers(main_pkg::poseArray_srv::Response pose_array)
    {
        visualization_msgs::Marker marker;
        marker.header.stamp = ros::Time::now();
        marker.ns = "bus_stops";
        marker.type = visualization_msgs::Marker::ARROW;
        marker.action = visualization_msgs::Marker::ADD;
        marker.scale.x = 1.0;
        marker.scale.y = 0.2;
        marker.scale.z = 0.2;
        marker.color.r = 0.0;
        marker.color.g = 1.0;
        marker.color.b = 0.0;
        marker.color.a = 1.0;
        marker.pose.orientation.x = 0;
        marker.pose.orientation.y = 0.7071;
        marker.pose.orientation.z = 0;
        marker.pose.orientation.w = 0.7071;

        marker.lifetime = ros::Duration();

        visualization_msgs::MarkerArray marker_array;
        for (int i = 0; i < pose_array.arr.poses.size(); i++)
        {
            marker.header.frame_id = pose_array.arr.header.frame_id;
            marker.id = i;
            marker.pose.position = pose_array.arr.poses[i].position;
            marker.pose.position.z += marker.scale.x;
            marker_array.markers.push_back(marker);
        }

        pub_marker.publish(marker_array);
        marker_array.markers.clear();
    }
    //Battery callback
    void batteryCallback(const kobuki_msgs::SensorState::ConstPtr &msg)
    {
        //Store variables for batterycheck()
        current_battery = msg->battery;
        current_dock_state = msg->charger;
    }

    bool battery_check() //Returns true if it needs to recharge
    {

        float batterypct = float(current_battery) / float(kobuki_max_charge) * 100; //Calculate prct
            
        if(int (current_dock_state) == 0 && batterypct < minimum_battery_pct){ //Not in dock and under 
            debug("battery time, mums ( ͡° ͜ʖ ͡°)");

 	    _client_receive_pose_kitchen.call(_srv_receive_pose_kitchen);

	    //_srv_receive_pose_kitchen.response.pose
	    
            //Kør til punkt foran dock
            //Start autodock 


            //system("roslaunch kobuki_auto_docking minimal.launch --screen");
            //system("roslaunch kobuk i_auto_docking activate.launch --screen");
            return true;
        }	
        return false;
    }

public:
    MoveBase() : MoveBaseClient("move_base", true)
    {
        
    }
};

//tydebugdef actionlib::SimpleActionClient<move_base_msgs::MoveBaseAction> Client;
int main(int argc, char *argv[])
{
    ros::init(argc, argv, "caterbot");
    MoveBase e;
    ros::Rate loop_rate(1);
    while (ros::ok)
    {
        if (e.job_size() == 0)
        {
	        if(!e.battery_check()){ //If it doesn't require recharging
                std::cout << "kikik" << std::endl;
                e._receive_pose_array();
            }
        }

        ros::spinOnce();
        loop_rate.sleep();
    }
    ros::spin();
}
