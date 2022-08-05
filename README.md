<!-- PROJECT LOGO -->
<br />
<div align="center">
  <a href="https://www.b-plus.com/de/home">
    <img src="https://www.b-plus.com/fileadmin/data_storage/images/b-plus_Logo.png" alt="Logo" width="150" height="150">
  </a>
  
  <h3 align="center">AVETO.app DataForwarding Measurement Object</h3>

  <p align="center">
    A sample implementation to be able to forward individual data streams from AVETO to external software and vice versa
    <br />
    <a href="#usage">View Usage</a>
    ·
    <a href="https://github.com/bplus-group/aveto_mo_data_forwarding/issues">Report Bug</a>
    ·
    <a href="https://github.com/bplus-group/aveto_mo_data_forwarding/issues">Request Feature</a>
  </p>
</div>
<br />

<!-- PROJECT SHIELDS -->
<div align="center">

  [![LinkedIn][linkedin-shield]][linkedin-url]
  [![Stars][star-shield]][star-url]

</div>

<!-- TABLE OF CONTENTS -->
<details>
  <summary>Table of Contents</summary>
  <ol>
    <li>
      <a href="#about-the-project">About The Project</a>
      <ul>
        <li><a href="#built-with">Built With</a></li>
      </ul>
    </li>
    <li>
      <a href="#getting-started">Getting Started</a>
      <ul>
        <li><a href="#prerequisites">Prerequisites</a></li>
        <li><a href="#installation">Installation</a></li>
      </ul>
    </li>
    <li><a href="#usage">Usage</a></li>
    <li><a href="#communication">Communication</a></li>
    <li><a href="#message-format">Message format</a></li>
    <li><a href="#contributing">Contributing</a></li>
    <li><a href="#license">License</a></li>
  </ol>
</details>

---

## About The Project

The project consists of three components: 
- Data Forwarding Measurement Object
- Data Backwarding Measurement Object
- Python example for the receiving side

### Data Forwarding Measurement Object

**Object:** Processor Object  
**Name:** Data Forwarding  
**Input:** Generic Connector "*"  
**Output:** -

**Properties:**

|Property Name	| Type |	Default	| Description|
|-|-|-|-|
| FPS Limit | uint32_t | 5 | FPS limit for the forwarding data stream |
| No Payload | bool | false |	**true:** Only meta data is forwarded <br /> **false:** Meta data and data is forwarded |

Once created, data packets coming in over a connection are forwarded to each connected client. By default, the forwarding frame rate is limited to 5 frames per second. This limit can be changed using the `FPS Limit` property.

By default, the complete packet data and additional metadata is forwarded. If you set the `No Payload` property to true, only the metadata is sent.


### Data Backwarding Measurement Object

**Object:** Processor Object  
**Name:** Data Backwarding  
**Input:** -  
**Output:** RGBA-Image and JSON meta data

After creation, the data sent from the client side is output through the output connectors.

### Python example

The project includes a small Python example that allows the user to receive data from the AVETO site, display it, and return data that has been processed.

<p align="right"><a href="#top">Back to top</a></p>

### Built With

**3rd party components measurement objects:**  
- cppzmq 4.8.1
- msgpack 3.3.0

**3rd party components python example:**  
See `example/requirements.txt`

<p align="right"><a href="#top">Back to top</a></p>

## Getting Started

Clone this repository and make sure you fulfill following requirements

### Prerequisites

- Installed AVETO.app >= 2.4.0
- Set AVETO_SDK_PATH as environment variable which by default points to the folder `C:\Program Files\b-plus\AVETO.app\sdk`
- Installed Windows SDK-Version 10.0.17763.0
- Installed *cppzmq* and *msgpack*
- The firewall must be set so that the AVETO Visualization host can be reached on the ports used for communication.
  - Default ports: 5770 - 5789 (TCP), 5870 - 5889 (TCP)

<p align="right"><a href="#top">Back to top</a></p>

### Installation

1. Install AVETO.app
2. Clone the repo
   ```sh
   git clone https://github.com/bplus-group/aveto_mo_data_forwarding.git
   ```
3. Adapt the include and library directories in the Visual Studio Solution that it finds the *cppzmq* and *msgpack* dependencies 
4. Build the project
5. Copy the DLLs to your AVETO MO directory (default: `%USERPROFILE%\Documents\AVETO\measobj\`)

<p align="right"><a href="#top">Back to top</a></p>

## Usage

Right-click in the node graph editor of the configuration manager and choose *Processor Objects -> Data Forwarding MO and Data Backwarding MO* as seen below.

![data_forwarding_mo][data_forwarding_mo]

Connect the input and output connectors of the objects according to your requirements. An example is shown below.  

![setup_example][setup_example]

### Use the included python example

1. Build the example container:

   ```bash
   docker build ./example/ -t data-forwarding-example
   ```

2. Determine the ip address of the AVETO Visualization host.

3. Run the example:

   ```bash
   docker run -it --rm \
       -e AVETO_HOST="<HOST_IP>" \
       data-forwarding-example
   
   # or with gui access   
    docker run -it --rm \
       -e AVETO_HOST="<HOST_IP>" \
       -e DISPLAY=$DISPLAY \
       -v /tmp/.X11-unix:/tmp/.X11-unix \
       data-forwarding-example
    
   ```

> **_NOTE:_**  You can also use the example without docker

> **_NOTE:_**  By default the forwarding is limited to 5fps (can be changed via properties)

<p align="right"><a href="#top">Back to top</a></p>

## Communication

ZeroMQ is used for forwarding and backwarding the data.
Every datasource gets its own channel/port (starts at 0, default limit: 20)

The following data can currently be forwarded:

- ```image``` -> RGBA camera images
- ```raw``` -> every other data type
- ```metadata_only``` -> no payload only metadata (can be enabled via mo properties)

If you want to receive data, you have to connect to a ZeroMQ PubSocket (default port 5770 + Channel (TCP)) and subscribe to the corresponding topic.

List of current available topics:

- ```out/image0```

If you want to send data, you have to connect to a PullSocket and send the corresponding message to it.

| Direction     | AVETO.vis socket type | AVETO.vis port | Application socket type |
|---------------|-----------------------|----------------|-------------------------|
| forwarding    | publisher socket      | 5770 + Channel | subscriber socket       |
| backwarding   | pull socket           | 5870 + Channel | push socket             | 

<p align="right"><a href="#top">Back to top</a></p>

## Message format

For encoding the messages MsgPack is used.

Message formats:

```
//////////////////////////////////
// Forwarding Message Definition:
//////////////////////////////////


//////////// Base MSG ////////////
1. Source     | string        | the source where the message originates from (default: "image0")
2. Timestamp  | unit64        | the timestamp of the AVETO data packet
3. Type       | string        | the type of the payload (currently supported: ["image", "raw", "metadata_only"])
4. Format     | string        | the format of the payload ("RGBA" for image type and the name of the connector in Vis. for all other types)

5. FormatSize | array<int, 3> | description of the the payload format structure (width, height, bits per pixel) (only used for "image")
6. Image      | bin format    | the payload as 1D byte array



//////////////////////////////////
// Backwarding Message Definition:
//////////////////////////////////


//////////// Base MSG ////////////
1. Target     | string        | the destination (output channel) to which the msg should go (default: "image0")
2. Timestamp  | unit64        | the timestamp of the AVETO data packet (used to match the results to original frames)
3. Type       | string        | the type of the payload (currently supported: ["image"])
4. Format     | string        | the format of the payload (currently supported: ["RGBA"])

//////////// RGBA MSG ////////////
5. FormatSize | array<int, 3> | description of the the payload format structure (width, height, bits per pixel)
6. MetaData   | string        | used for the meta data channel (JSON format, e.g. for list of detections, ...)
7. Image      | bin format    | the image payload as 1D byte array

```

<p align="right"><a href="#top">Back to top</a></p>

## Contributing

If you have a suggestion that would improve this, please fork the repo and create a pull request. You can also simply open an issue with the tag "enhancement".  
Don't forget to give the project a star! Thanks again!

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/NewFeature`)
3. Commit your Changes (`git commit -m 'Add some NewFeature'`)
4. Push to the Branch (`git push origin feature/NewFeature`)
5. Open a Pull Request

<p align="right"><a href="#top">Back to top</a></p>

## License

Check License information. See `LICENSE` for more information.

<p align="right"><a href="#top">Back to top</a></p>




<!---Links And Images -->
[linkedin-shield]: https://img.shields.io/badge/-LinkedIn-black.svg?style=for-the-badge&logo=linkedin&color=808080
[linkedin-url]: https://de.linkedin.com/company/b-plus-group
[star-shield]: https://img.shields.io/github/stars/bplus-group/aveto_mo_data_forwarding.svg?style=for-the-badge&color=144E73&labelColor=808080
[star-url]: https://github.com/bplus-group/aveto_mo_data_forwarding
[data_forwarding_mo]: ./docs/images/data_forwarding_mo.png "DataForwardingMO"
[setup_example]: ./docs/images/aveto_vis_example.png "Example AVETO Visualization setup"
