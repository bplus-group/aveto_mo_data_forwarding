import os
import zmq
import msgpack
from io import BytesIO
import numpy as np
import cv2


AVETO_HOST = os.getenv('AVETO_HOST')
RECV_CHANNEL = 0
SEND_CHANNEL = 0

context = zmq.Context()

sub_socket = context.socket(zmq.SUB)
sub_socket.connect(f"tcp://{AVETO_HOST}:{5770+RECV_CHANNEL}")
sub_socket.setsockopt_string(zmq.SUBSCRIBE, "out")

push_socket = context.socket(zmq.PUSH)
push_socket.connect(f"tcp://{AVETO_HOST}:{5870+SEND_CHANNEL}")

while 1:
    parts = sub_socket.recv_multipart()
    print("TOPIC:", parts[0].decode("utf-8"))

    buf = BytesIO()
    buf.write(parts[1])
    buf.seek(0)

    unpacker = msgpack.Unpacker(buf, raw=False)
    msg_source = unpacker.__next__()
    msg_timestamp = unpacker.__next__()
    msg_type = unpacker.__next__()
    msg_format = unpacker.__next__()
    msg_format_size = unpacker.__next__()
    msg_data = unpacker.__next__()
    msg_data_size = len(msg_data)

    print("Source:", msg_source)
    print("Timestamp:", msg_timestamp)
    print("Type:", msg_type)
    print("Format:", msg_format)
    print("Format Size:", msg_format_size)
    print("")

    
    # Determine data type

    if msg_type == "image" and msg_format == "RGBA":
        print(f"Detected data type: Image (RGBA)")

        image_raw_size = msg_format_size[1] * msg_format_size[0] * msg_format_size[2]/8

        if msg_data_size == image_raw_size:                
            decoded = np.frombuffer(msg_data, dtype=np.uint8)
            bgr_img = decoded.reshape((msg_format_size[1], msg_format_size[0], int(msg_format_size[2]/8)))
            rgb_img = cv2.cvtColor(bgr_img, cv2.COLOR_BGR2RGB)
        else:
            print(f"Error: Image Size does not match {msg_data_size} != {image_raw_size}")  
            continue          
            
    elif msg_type == "raw" and "IMAGE_YUV422_8BPP" in msg_format:
        print(f"Detected data type: Image (YUV422_8BPP)")

        image_raw_size = msg_format_size[1] * msg_format_size[0] * (msg_format_size[2]/8)

        if msg_data_size == image_raw_size:
            decoded = np.frombuffer(msg_data, dtype=np.uint8)
            decoded = decoded.reshape((msg_format_size[1], msg_format_size[0], int(msg_format_size[2]/8)))
            bgr_img = cv2.cvtColor(decoded, cv2.COLOR_YUV2RGB_Y422)
            rgb_img = cv2.cvtColor(bgr_img, cv2.COLOR_BGR2RGB)
        else:
            print(f"Error: Image Size does not match {msg_data_size} != {image_raw_size}")
            continue

    else: 
        # if data type is not an image -> continue
        continue
        
    ### Display image with OpenCV ###

    # cv2.imshow(msg_source, rgb_img)
    # cv2.waitKey(1)

    ### Build backwarding message ###

    rgba_img = cv2.cvtColor(rgb_img, cv2.COLOR_BGR2RGBA)
    msg_format_size[2] = 32    
    
    packer = msgpack.Packer(autoreset=False)
    packer.pack("image0")                       # Target
    packer.pack(msg_timestamp)                  # Timestamp
    packer.pack("image")                        # Type
    packer.pack("RGBA")                         # Format
    packer.pack(msg_format_size)                # Format Size
    packer.pack("{detections: 3}")              # Meta Data
    packer.pack(rgba_img.tobytes())             # Payload

    # send message
    push_socket.send(packer.bytes())





