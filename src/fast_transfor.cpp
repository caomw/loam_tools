#include "laser_geometry/laser_geometry.h"
#include "tf/tf.h"
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/timer.hpp>

#include <pcl_ros/transforms.h>
#include<pcl_ros/point_cloud.h>
#include <fstream>

#include <sys/shm.h>
#include <sys/sem.h>

ros::Subscriber sub;
ros::Publisher pub;


boost::asio::io_service io;
boost::asio::serial_port sp(io,"/dev/ttyUSB0");
double w=0;
int last_sum;
double last_endtime;

//for debug
std::fstream fout;
std::fstream foutlaser;

double time_before,time_now;
double sum_before,sum_now;

static double scan_time_old=0;

void handle_read(char * buf,boost::system::error_code ec, std::size_t bytes_transferred){
    if(bytes_transferred > 4){
        double sum;
       // static tf::TransformBroadcaster br;
        //tf::Transform transform;
        sum = ((0xff &(*(buf+2))) * 255)+((0xff & (*(buf+3))));

        if(sum > -1 && sum <721){
            //ROS_INFO("sum is:");
            //std::cout << sum << std::endl;
            int shmid;
            shmid = shmget(111,4,0666);
            int * shm, *s;
            shm = (int*)shmat(shmid,NULL,0);
            s = shm;
            *s = sum;

//            transform.setOrigin(tf::Vector3(0.0,0.0,0.0));
//            tf::Quaternion q;
//            q.setRPY(0,0,(3.1415926 * ((sum / 4) - 90)/180));
//            transform.setRotation(q);
//            br.sendTransform(tf::StampedTransform(transform,ros::Time::now(),"/camerat","/laser"));
        }
    }
}


void lCallback(const sensor_msgs::LaserScan::ConstPtr& scan_msg)
{

    // double gettime = ros::Time::now().toSec();

    //ROS_INFO("1");
    laser_geometry::LaserProjection p;

    sensor_msgs::PointCloud2 pointcloud_tmp;

    double scan_time,scan_diff,point_scan_diff,point_cloud_time;

    scan_time= scan_msg->header.stamp.toSec();
    scan_diff=scan_time-scan_time_old;
    scan_time_old=scan_time;
    p.projectLaser(*scan_msg,pointcloud_tmp,-1.0,3);//1.0 - 2.0  or -1.0

    point_cloud_time = pointcloud_tmp.header.stamp.toSec();

    //std::cout << "scantime is :;:::"<<scan_time <<std::endl;
    //std::cout << "pointclou is :::::"<< point_cloud_time <<std::endl;
    point_scan_diff=point_cloud_time-scan_time;
    pointcloud_tmp.header.frame_id="/camera";

    Eigen::Matrix4f Tm= Eigen::Matrix4f::Identity();
    Eigen::Matrix4f Tm1 = Eigen::Matrix4f::Identity();
    Eigen::Matrix4f Tm2 = Eigen::Matrix4f::Identity();
    Eigen::Matrix4f Tm3 = Eigen::Matrix4f::Identity();

    //ROS_INFO("2----");

    // set the w to get angle
    bool isok(false);
    char bufread[10];
    int a(0),b(0),c(0),sum;
    sum = 0;
    int times(0);
    double endtime;
    while(isok==false)//ver 1.0--read
    {
        memset(bufread,0,10);
        //bool err(false);
        //if((sp.is_open()))
        //{
         //   ROS_INFO("2-is open---");
        //    boost::asio::read(sp,boost::asio::buffer(bufread));
        //}else{
        //    ROS_INFO("sp error");
       // }

        int shmid;
        int * shm,*s;
        shmid = shmget(111,4,0666);
        shm = (int*)shmat(shmid,NULL,0);

        s = shm;
        sum = *s;

        //ROS_INFO("2-0---");
        endtime = ros::Time::now().toSec();
        time_now = endtime;
        if (sum < 720){
            isok = true;
            sum_now = sum ;
            break;
        }else{
            std::cout << "error sum:"<< sum<< std::endl;
        }







        //if(!isok) return;
        times++;
        if(times > 1) return;

    }
    //std::cout << "times = " << times <<std::endl;

    //ROS_INFO("3");
    endtime = endtime - pointcloud_tmp.header.stamp.toSec();

    //recover the sum

    fout <<scan_diff<<","<<point_scan_diff <<","<<(sum/4-90) <<","<<times<<","<<endtime<<",";
    int nsum;
    double avg_v;
    //std::cout << "endtime is :;:::"<<endtime <<std::endl;
    avg_v = (((sum_before - sum_now)/(time_before - time_now)))/720;
    //std::cout <<"avg_v="<<avg_v<<std::endl;
    if(sum >= last_sum)
    {
        nsum = sum - (endtime - 0.01) * std::abs((sum_before - sum_now)/(time_before - time_now));//700;//(sum - last_sum)/(endtime - last_endtime);//720;
        if(nsum < 0)
        {

            nsum = -1 * nsum ;
        }
    }else{
        nsum = sum + (endtime - 0.01) * std::abs((sum_before - sum_now)/(time_before - time_now));//700;//(su690m - last_sum)/(endtime - last_endtime);//720;
        if(nsum > 720)
        {

            nsum = 720 - (nsum -720) ;
        }
    }

    time_before = time_now;
    sum_before = sum_now;
    fout << (nsum/4 -90 )<<","<<(nsum-sum)/4 <<","<<avg_v<<","<<pointcloud_tmp.width * pointcloud_tmp.height<<std::endl;
    sum = nsum;




    //w is the angle of lidar
    w =(sum /4 )-90 ;
    last_endtime = endtime;
    last_sum = sum;
    float theta=3.1415926 * w / 180;
    //std::cout <<"theta:::::::"<<theta<<std::endl;

    //    Tm1(0,0)=cos(theta);
    //    Tm1(0,1)=-sin(theta);
    //    Tm1(1,0)=sin(theta);
    //    Tm1(1,1)=cos(theta);
    //    Tm1(2,3)=2.5;
    //    Tm1(1,3)=2.5;
    //    Tm1(3,3)=1;


    //ROS_INFO("%f",theta);
    //ROS_INFO("w is:%f",w);

    Tm(0,0)=1;
    Tm(1,1)=cos(theta);
    Tm(1,2)=-sin(theta);
    Tm(2,1)=sin(theta);
    Tm(2,2)=cos(theta);


    Tm(2,3)=cos(theta) * 0.04;// * 50;//axis-z
    Tm(1,3)=-sin(theta) * 0.04 ;//* 50;//axis-y
    Tm(3,3)=1;



    //z -90
    Tm3(0,1)=1;
    Tm3(1,0)=-1;
    Tm3(2,2)=1;

    //x -90
    Tm2(0,0)=1;
    Tm2(1,2)=1;
    Tm2(2,1)=-1;
    // ROS_INFO("4");
    //z +180
    Tm1(0,0)=-1;
    Tm1(1,1)=-1;
    Tm1(2,2)=1;


    //std::cout<< "pointcloud data:"<<pointcloud_tmp.data[20]<<"   size:"<<pointcloud_tmp.data.size()<<std::endl;

    //std::cout <<"list"<<pcl::getFieldsList(pointcloud_tmp)<<std::endl;
    //get the really y z of the point
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    ///
    ///
    ///
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    //   //out
    sensor_msgs::PointCloud2 out;
    //   //define a transfomr (may same as Tm)
    Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
    transform = Tm;
    //   transform(0,0)=1;
    //   transform(1,1)=cos(theta);
    //   transform(1,2)=-sin(theta);
    //   transform(2,1)=sin(theta);
    //   transform(2,2)=cos(theta);
    //   transform(2,3)=cos(theta) * 0.035;
    //   transform(1,3)=-sin(theta) * 0.035;
    //   transform(3,3) = 1;
    // Get X-Y-Z indices;
    int x_idx = pcl::getFieldIndex(pointcloud_tmp,"x");
    int y_idx = pcl::getFieldIndex(pointcloud_tmp,"y");
    int z_idx = pcl::getFieldIndex(pointcloud_tmp,"z");


    //check if distance is available
    int dist_idx = pcl::getFieldIndex(pointcloud_tmp,"distance");
    //xyz_offset
    Eigen::Array4i xyz_offset (pointcloud_tmp.fields[x_idx].offset,pointcloud_tmp.fields[y_idx].offset,pointcloud_tmp.fields[z_idx].offset,0);
    double ntheta;
    int static ok=0;
    for (size_t i = 0;i<pointcloud_tmp.width * pointcloud_tmp.height;++i)
    {

        Eigen::Vector4f pt(*(float *)&pointcloud_tmp.data[xyz_offset[0]],*(float*)&pointcloud_tmp.data[xyz_offset[1]],*(float*)&pointcloud_tmp.data[xyz_offset[2]],1);
        Eigen::Vector4f pt_out;




        ntheta = theta-((90-atan(pt[0] / pt[1]))/270*0.025*avg_v);//-;
        //theta = 0;
        transform(0,0)=1;
        transform(1,1)=cos(ntheta);
        transform(1,2)=-sin(ntheta);
        transform(2,1)=sin(ntheta);
        transform(2,2)=cos(ntheta);
        transform(2,3)=cos(ntheta) * 0.035;
        transform(1,3)=-sin(ntheta) * 0.035;
        transform(3,3) = 1;





        bool max_range_point = false;
        int distance_ptr_offset = i * pointcloud_tmp.point_step + pointcloud_tmp.fields[dist_idx].offset;
        float * distance_ptr = (dist_idx<0?NULL:(float*)(&pointcloud_tmp.data[distance_ptr_offset]));
        if(!std::isfinite(pt[0]) || !std::isfinite(pt[1]) || !std::isfinite(pt[2]))
        {
            if (distance_ptr == NULL || !std::isfinite(*distance_ptr))   //Invalid point
            {
                pt_out = pt;
            }else{//max range point
                pt[0] = *distance_ptr; //Replace x with the x value saved in distance
                pt_out = transform * pt;//
                max_range_point = true;
            }
        }else{
            pt_out = transform * pt;
        }

        if(max_range_point)
        {
            //Save x. value in distance again
            *(float*)(&pointcloud_tmp.data[distance_ptr_offset]) = pt_out[0];
            pt_out[0] = std::numeric_limits<float> ::quiet_NaN();
        }

        memcpy(&pointcloud_tmp.data[xyz_offset[2]],&pt_out[0], sizeof(float));
        memcpy(&pointcloud_tmp.data[xyz_offset[0]],&pt_out[1], sizeof(float));
        memcpy(&pointcloud_tmp.data[xyz_offset[1]],&pt_out[2], sizeof(float));

        xyz_offset += pointcloud_tmp.point_step;


    }

    ok++;



    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    ///

    //pcl_ros::transformPointCloud(Tm,pointcloud_tmp,pointcloud_tmp);

    //sensor_msgs::PointCloud2 r_out_tmp;
    //pcl_ros::transformPointCloud(Tm3,pointcloud_tmp,pointcloud_tmp);
    ///pcl_ros::transformPointCloud(Tm3,pointcloud_tmp,r_out_tmp);

    //pcl_ros::transformPointCloud(Tm2,pointcloud_tmp,pointcloud_tmp);
    //pcl_ros::transformPointCloud(Tm2,r_out_tmp,pointcloud_tmp);


    ROS_INFO("5");

    pub.publish(pointcloud_tmp);

}

int main(int argc,char **argv)
{
    ros::init(argc,argv,"fast_transfor");
    ros::NodeHandle n;
    ROS_INFO("Now,start the node succeful!");



    fout.open("/home/lixin/data.csv");
    foutlaser.open("/home/lixin/datalaser.csv");
    std::cout <<"....................1....................."<<std::endl;

    //create a shm
    int *shm,*s;
    int shmid;

    if((shmid = shmget(111,4,IPC_CREAT|0666))<0)
        std::cout <<"shmget error!";
    if((shm = (int*)shmat(shmid,NULL,0)) == (int*) -1)
        std::cout <<"shmat error!";




    sp.set_option(boost::asio::serial_port::baud_rate(115200));
    sp.set_option(boost::asio::serial_port::flow_control());
    sp.set_option(boost::asio::serial_port::parity());
    sp.set_option(boost::asio::serial_port::stop_bits());
    sp.set_option(boost::asio::serial_port::character_size(8));

    ////////begin
    boost::asio::write(sp,boost::asio::buffer("\xAA",1));
    boost::asio::write(sp,boost::asio::buffer("\x55",1));
    boost::asio::write(sp,boost::asio::buffer("\x0A",1));
    boost::asio::write(sp,boost::asio::buffer("\x01",1));
    boost::asio::write(sp,boost::asio::buffer("\x0B",1));

    ROS_INFO("step2111");

    pub=n.advertise<sensor_msgs::PointCloud2>("sync_scan_cloud_filtered",1);
    sub=n.subscribe("first",1,lCallback);





    while(ros::ok()){
        ros::spinOnce();
        char buf[5];
        boost::asio::io_service iosev;
        boost::asio::serial_port sp_tmp(iosev,"/dev/ttyUSB0");
        memset(buf,0,5);
        boost::asio::async_read(sp_tmp,boost::asio::buffer(buf),boost::bind(handle_read,buf,_1,_2));
        boost::asio::deadline_timer timer(iosev);

        timer.expires_from_now(boost::posix_time::millisec(8));
        timer.async_wait(boost::bind(&boost::asio::serial_port::cancel,boost::ref(sp_tmp)));


        iosev.run();
        sp_tmp.close();
        for (int kk = 1;kk<100000;kk++);




    }
    std::cout <<"....................2....................."<<std::endl;
    //std::cout <<"....................2....................."<<std::endl;
    ////////stopAA550A020C
    boost::asio::write(sp,boost::asio::buffer("\xAA",1));
    boost::asio::write(sp,boost::asio::buffer("\x55",1));
    boost::asio::write(sp,boost::asio::buffer("\x0A",1));
    boost::asio::write(sp,boost::asio::buffer("\x02",1));
    boost::asio::write(sp,boost::asio::buffer("\x0c",1));
    sleep(2);
    std::cout <<"....................2....................."<<std::endl;
    //std::cout <<"....................2....................."<<std::endl;
    ////////stopAA550A020C
    boost::asio::write(sp,boost::asio::buffer("\xAA",1));
    boost::asio::write(sp,boost::asio::buffer("\x55",1));
    boost::asio::write(sp,boost::asio::buffer("\x0A",1));
    boost::asio::write(sp,boost::asio::buffer("\x02",1));
    boost::asio::write(sp,boost::asio::buffer("\x0c",1));
    sleep(2);
    std::cout <<"....................2....................."<<std::endl;
    //std::cout <<"....................2....................."<<std::endl;
    ////////stopAA550A020C
    boost::asio::write(sp,boost::asio::buffer("\xAA",1));
    boost::asio::write(sp,boost::asio::buffer("\x55",1));
    boost::asio::write(sp,boost::asio::buffer("\x0A",1));
    boost::asio::write(sp,boost::asio::buffer("\x02",1));
    boost::asio::write(sp,boost::asio::buffer("\x0c",1));
    fout.close();
    foutlaser.close();
    sp.close();
    return 0;
}



