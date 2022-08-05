/******************************************************************************/
/*! \file
*
* \verbatim
******************************************************************************
*                                                                            *
*    Copyright (c) 2015-2022, b-plus technologies GmbH.                      *
*                                                                            *
*    All rights are reserved by b-plus technologies GmbH.                    *
*    The Customer is entitled to modify this software under his              *
*    own license terms.                                                      *
*                                                                            *
*    You may use this code according to the license terms of b-plus.         *
*    Please contact b-plus at services@b-plus.com to get the actual          *
*    terms and conditions.                                                   *
*                                                                            *
******************************************************************************
\endverbatim
*
* \brief Data Forwarding MO
* \author Christopher Maneth
* \copyright (C)2015-2022 b-plus technologies GmbH
* \date 04.05.2022
* \version 2.4
*
******************************************************************************/

#include "ProcessorMO.h"

using namespace std::chrono_literals;


CProcessorObject::CProcessorObject() :
	m_uiFPSLimit(5),
	m_bNoPayload(false),
	m_uiInputWidth(0),
	m_uiInputHeight(0),
	m_uiInputBPP(0),
	m_bInputIsRGBA(false),
	m_ZeroMQThread(),
	m_ZmqSock(m_ZmqCtx, zmq::socket_type::pub),
	m_bZeroMQActive(false),
	m_iZmqChannel(0)
{
}

AVETO::Core::TStatus CProcessorObject::Initialize()
{
	try
	{
		m_ZmqSock.setsockopt(ZMQ_SNDHWM, 3);

		for (m_iZmqChannel = 0; m_iZmqChannel < ZEROMQ_CHANNEL_LIMIT; m_iZmqChannel++)
		{
			try
			{
				m_iZmqPort = ZEROMQ_START_PORT + m_iZmqChannel;
				auto ssListen = std::string(ZEROMQ_LISTEN) + std::to_string(m_iZmqPort);
				m_ZmqSock.bind(ssListen);
				auto ssObjectName = std::string("Data Forwarding CH") + std::to_string(m_iZmqChannel);
				this->SetName(ssObjectName.c_str());
				break;
			}
			catch (std::exception e)
			{
			}
		}

		m_bZeroMqActive = true;
		m_ZeroMQThread = std::thread(&CProcessorObject::ZeroMQLoop, this);
	}
	catch (std::exception e)
	{
		return AVETO_S_FALSE;
	}

	return AVETO::Dev::Support::CAvetoProcessorObject::Initialize();
}

AVETO::Core::TStatus CProcessorObject::Terminate()
{
	// Reset the initialized flag.
	AVETO::Dev::Support::CAvetoProcessorObject::Terminate();

	m_bZeroMqActive = false;
	if (m_ZeroMQThread.get_id() != std::thread::id()) 
		m_ZeroMQThread.join();

	m_ZmqCtx.close();

	return AVETO_S_OK;
}

int CProcessorObject::ZeroMQLoop()
{
	while (m_bZeroMqActive)
	{
		auto target_fps = m_uiFPSLimit;
		if (target_fps < 1)
			target_fps = 1;

		std::this_thread::sleep_for(1000ms / target_fps);						// limit to max FPS

		{ // lock
			std::unique_lock<std::mutex> lock(m_mtxPacketQueue);

			// remove all but newest item from queue
			while (m_queuePackets.size() > 1)
			{
				m_queuePackets.pop();
			}

			if (!m_queuePackets.empty())
			{
				if(BuildMsgBuffer())
					SendMsgBuffer();

				m_queuePackets.pop();
			}
		}
	}

	m_ZmqSock.close();

	return 0;
}

bool CProcessorObject::BuildMsgBuffer()
{
	const auto frame = m_queuePackets.front();

	const std::string ssSource = ZEROMQ_SOURCE;
	std::string ssType = "raw";
	std::string ssFormat = "RAW";
	std::array<int, 3> aFormatSize{ { 0, 0, 0 } };
	uint32_t uiNumBytes = 0;

	if (m_bNoPayload)												// METADATA_ONLY
	{
		ssType = "metadata_only";
	}
	else if (m_bInputIsRGBA)										// RGBA
	{
		uiNumBytes = m_uiInputWidth * m_uiInputHeight * 4;

		if (uiNumBytes != frame->GetDataLen())
			return false;  // invalid package size for type
				
		ssType = "image";
		ssFormat = "RGBA";
		aFormatSize[0] = m_uiInputWidth;
		aFormatSize[1] = m_uiInputHeight;
		aFormatSize[2] = 32;		
	}
	else															// RAW / OTHER
	{
		uiNumBytes = frame->GetDataLen();
		ssFormat = m_ssInputName;
		aFormatSize[0] = m_uiInputWidth;
		aFormatSize[1] = m_uiInputHeight;
		aFormatSize[2] = m_uiInputBPP;
	}

	msgpack::packer<msgpack::sbuffer> packer(&m_MsgBuffer);
	packer.pack(ssSource);
	packer.pack(frame->GetTimestamp());
	packer.pack(ssType);
	packer.pack(ssFormat);
	packer.pack(aFormatSize);
	packer.pack_bin(uiNumBytes);
	if (uiNumBytes > 0)
	{
		packer.pack_bin_body(static_cast<const char*>(frame->GetData()), uiNumBytes);
	}

	return true;
}

void CProcessorObject::SendMsgBuffer()
{
	const std::string topicStr = ZEROMQ_TOPIC;
	zmq::message_t topic(topicStr.length());
	zmq::message_t msg(m_MsgBuffer.size());

	memcpy(topic.data(), topicStr.data(), topicStr.size());
	memcpy(msg.data(), m_MsgBuffer.data(), m_MsgBuffer.size());

	m_ZmqSock.send(topic, ZMQ_SNDMORE);
	m_ZmqSock.send(msg);

	m_MsgBuffer.clear();
}

void CProcessorObject::OnConnect(const AVETO::Core::SConnectionEvent& rsConnectInfo)
{
	HandleConnectorChange(rsConnectInfo.tOutConnectorID);
}

void CProcessorObject::OnConnectedConnectorChanged(const char* szOwnConnectorName, uint32_t uiOwnConnectorFlags, AVETO::Core::TObjID tConnectedConnectorID)
{
	// Read the image properties from the connector.
	AvCore::IObjectPtr ptrConnector = AvCore::GetInstanceFromGlobal(tConnectedConnectorID);

	// Check whether this is the input connector.
	if (!AvCore::CompareRestrictedBitmask(uiOwnConnectorFlags, AVETO::Core::EConnectorFlags::direction_flag,
		AVETO::Core::EConnectorFlags::direction_in))
	{
		return;
	}

	HandleConnectorChange(tConnectedConnectorID);	
}

void CProcessorObject::ProcessData(const AVETO::Core::SDataPacket* rgsPackets, uint32_t uiPackets)
{
	if (!uiPackets) return;
	if (!rgsPackets) return;

	// lock
	std::unique_lock<std::mutex> lock(m_mtxPacketQueue);

	std::shared_ptr<AvCore::SDataPacketPtr> ptrRgsPacket(new AvCore::SDataPacketPtr());
	ptrRgsPacket->Set(*rgsPackets);

	m_queuePackets.push(ptrRgsPacket);
}

void CProcessorObject::HandleConnectorChange(AVETO::Core::TObjID tConnectedConnectorID)
{
	// lock to prevent handling queue packages incorrectly
	std::unique_lock<std::mutex> lock(m_mtxPacketQueue);

	m_ssInputName = AvCore::GetProp<std::string>(tConnectedConnectorID, "Alias");
	if (m_ssInputName.empty())
	{
		m_ssInputName = AvCore::GetProp<std::string>(tConnectedConnectorID, "Name");		
	}

	m_uiInputWidth = AvCore::GetProp<uint32_t>(tConnectedConnectorID, "Width");
	m_uiInputHeight = AvCore::GetProp<uint32_t>(tConnectedConnectorID, "Height");
	m_uiInputBPP = AvCore::GetProp<uint32_t>(tConnectedConnectorID, "BPP");
	m_bInputIsRGBA = IsValidRGBA(tConnectedConnectorID);
	
	// clear packet queue
	while (m_queuePackets.size() > 0)
	{
		m_queuePackets.pop();
	}
}

bool CProcessorObject::IsValidRGBA(AVETO::Core::TObjID tConnectedConnectorID) const
{
	const uint8_t uiFormatStandard =
		(static_cast<uint32_t>(
			AvCore::GetProp<AVETO::Core::EImageInterpretFlags>(tConnectedConnectorID, "Interpretation")
			) >> 24) & 0xff;

	const auto Id = AvCore::GetProp<genicam_helper::PfncFormat_>(tConnectedConnectorID, "Image ID");
	if (uiFormatStandard == 0)
	{
		return false;	// not supported
	}

	if (Id != genicam_helper::RGBa8)
	{
		return false;	// not supported
	}

	// RGBa8 means:
	// m_eFormat = RGBA;
	// m_uiBPP = 32;
	// m_bUseBGR = false;
	// m_eEndianess = BigEndian;

	return true;
}
