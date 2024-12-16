#!/usr/bin/env python3
"""
    Script to extract the XYZ coordinates from a Pointcloud2 message

    Version 0.2
    2021-12-01
"""

import os
import argparse
import numpy as np

# The following two imports require that ROS be installed on the machine
import rosbag
import sensor_msgs.point_cloud2 as pc2

if __name__ == "__main__":

    parser = argparse.ArgumentParser(description='Extract XYZ from Pointcloud2 message.')

    parser.add_argument("filename",
                        help="path and filename of rosbag",
                        type=str)
    parser.add_argument("-t", dest="topic_name", required=False, default='/lumotive_ros/pointcloud',
                        help="topic name to extract (default: /lumotive_ros/pointcloud)",
                        type=str)
    parser.add_argument("-n", dest="frame_number", required=False, default=0,
                        help="frame (or message) number to extract (default: 0)",
                        type=int)

    args = parser.parse_args()
    filename = args.filename
    topic_name = args.topic_name
    frame_num = args.frame_number

    print("Opening the rosbag file.")
    if not os.path.exists(filename):
        raise Exception("Failed to open " + filename + ", it does not exists.")

    if not filename.endswith(".bag"):
      raise Exception("Please make sure that the file pointed to is a ROSBAG (file extension should be .bag).")

    points = []
    print("Reading the rosbag file.")
    with rosbag.Bag(filename, 'r') as bag:

        # Check if the topic exists in the rosbag
        available_topics = list(bag.get_type_and_topic_info()[1].keys())
        if topic_name not in available_topics:
            raise Exception("Requested topic name: " + str(topic_name) + " not available. Choices are: "
                            + str(available_topics))

        msg_num = 0
        for topic, msg, t in bag.read_messages(topics=[topic_name]):

            if msg_num != frame_num:
                msg_num += 1
                continue

            width = msg.width
            height = msg.height
            for point in pc2.read_points(msg, skip_nans=True):
                points.append(point[0:4])

            break

    if not points:  # List is empty, could not parse message
        raise Exception("Could not extract message " + str(frame_num) + " from topic " + topic_name + " for which "
                        + str(msg_num) + " messages have been recorded.")

    points = np.asarray(points)

    print("Extracted frame " + str(frame_num) + " of size " + str(height) + "x" + str(width) + ". Exporting to CSV.")
    # Remove filename extension to replace it with CSV
    filename = os.path.splitext(filename)[0] + '.csv'
    header = 'X Y Z intensity'

    np.savetxt(filename, points, fmt='%f', delimiter=' ', header=header)

    print('All done.')
