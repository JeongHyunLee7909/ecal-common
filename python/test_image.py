#!/usr/bin/env python3

import sys
import time

import capnp
import numpy as np
import cv2

import ecal.core.core as ecal_core
from ecal.core.subscriber import ByteSubscriber

capnp.add_import_hook(['../src/capnp'])

import image_capnp as eCALImage

imshow_map = {}

def callback(topic_name, msg, ts):

    # need to remove the .decode() function within the Python API of ecal.core.subscriber StringSubscriber
    with eCALImage.Image.from_bytes(msg) as imageMsg:
        print(f"seq = {imageMsg.header.seq}, with {len(msg)} bytes, encoding = {imageMsg.encoding}")
        print(f"width = {imageMsg.width}, height = {imageMsg.height}")
        print(f"exposure = {imageMsg.exposureUSec}, gain = {imageMsg.gain}")

        if (imageMsg.encoding == "mono8"):

            mat = np.frombuffer(imageMsg.data, dtype=np.uint8)
            mat = mat.reshape((imageMsg.height, imageMsg.width, 1))

            imshow_map["mono8"] = mat

            # cv2.imshow("mono8", mat)
            # cv2.waitKey(3)
        elif (imageMsg.encoding == "yuv420"):
            mat = np.frombuffer(imageMsg.data, dtype=np.uint8)
            mat = mat.reshape((imageMsg.height * 3 // 2, imageMsg.width, 1))

            mat = cv2.cvtColor(mat, cv2.COLOR_YUV2BGR_IYUV)

            imshow_map["yuv420"] = mat
            # cv2.imshow("yuv420", mat)
            # cv2.waitKey(3)
        elif (imageMsg.encoding == "bgr8"):
            mat = np.frombuffer(imageMsg.data, dtype=np.uint8)
            mat = mat.reshape((imageMsg.height, imageMsg.width, 3))
            imshow_map["bgr8"] = mat
        elif (imageMsg.encoding == "jpeg"):
            mat_jpeg = np.frombuffer(imageMsg.data, dtype=np.uint8)
            mat = cv2.imdecode(mat_jpeg, cv2.IMREAD_COLOR)
            imshow_map["jpeg"] = mat
        else:
            raise RuntimeError("unknown encoding: " + imageMsg.encoding)


def main():  
    # mat = np.ones((800,1280,1), dtype=np.uint8) * 125
    # cv2.imshow("mono8", mat)
    # cv2.imwrite("test0.jpg", mat)

    # print eCAL version and date
    print("eCAL {} ({})\n".format(ecal_core.getversion(), ecal_core.getdate()))
    
    # initialize eCAL API
    ecal_core.initialize(sys.argv, "test_image_sub")
    
    # set process state
    ecal_core.set_process_state(1, 1, "I feel good")

    # create subscriber and connect callback
    # sub = ByteSubscriber("S0/camd")
    sub = ByteSubscriber("raw_fisheye_image")
    # sub = ByteSubscriber("S0/stereo1_l")
    sub.set_callback(callback)
    
    # idle main thread
    while ecal_core.ok():
        for im in imshow_map:
            cv2.imshow(im, imshow_map[im])
        cv2.waitKey(3)
        # time.sleep(0.01)
        
    
    # finalize eCAL API
    ecal_core.finalize()

if __name__ == "__main__":
    main()