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
	m_uiOutputWidth(0),
	m_uiOutputHeight(0),
	m_ZeroMQRecvThread(),
	m_ZmqRecvSock(m_ZmqCtx, zmq::socket_type::pull),
	m_bZeroMQActive(false),
	m_iZmqChannel(0)
{
}

AVETO::Core::TStatus CProcessorObject::Initialize()
{
	try
	{
		m_connOutImg.SetGenICamId(genicam_helper::RGBa8);

		m_ZmqRecvSock.setsockopt(ZMQ_LINGER, 0);
		for (m_iZmqChannel = 0; m_iZmqChannel < ZEROMQ_CHANNEL_LIMIT; m_iZmqChannel++)
		{
			try
			{
				m_iZmqPort = ZEROMQ_START_PORT + m_iZmqChannel;
				auto ssListen = std::string(ZEROMQ_LISTEN) + std::to_string(m_iZmqPort);
				m_ZmqRecvSock.bind(ssListen);
				auto ssObjectName = std::string("Data Backwarding CH") + std::to_string(m_iZmqChannel);
				this->SetName(ssObjectName.c_str());
				break;
			}
			catch (std::exception e)
			{
			}
		}

		m_bZeroMqActive = true;
		m_ZeroMQRecvThread = std::thread(&CProcessorObject::ZeroMQRecvLoop, this);
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
	if (m_ZeroMQRecvThread.get_id() != std::thread::id())
		m_ZeroMQRecvThread.join();

	m_ZmqCtx.close();

	return AVETO_S_OK;
}

int CProcessorObject::ZeroMQRecvLoop()
{
	while (m_bZeroMqActive)
	{
		try
		{
			zmq::message_t msg;
			auto size = m_ZmqRecvSock.recv(msg, zmq::recv_flags::dontwait);
			if (!size.has_value())
			{
				std::this_thread::sleep_for(3ms);
				continue;
			}

			std::size_t totalOffset = 0;

			msgpack::unpacked targetUnpacked;
			std::size_t targetOffset = 0;
			msgpack::unpack(targetUnpacked, (const char*)msg.data(), msg.size(), targetOffset);
			totalOffset += targetOffset;
			msgpack::object targetObj = targetUnpacked.get();
			auto target = targetObj.as<std::string>();

			if (target != "image0")
			{
				continue;
			}

			msgpack::unpacked tsUnpacked;
			std::size_t tsOffset = 0;
			msgpack::unpack(tsUnpacked, (const char*)msg.data() + totalOffset, msg.size() - totalOffset, tsOffset);
			totalOffset += tsOffset;
			msgpack::object tsObj = tsUnpacked.get();
			auto timestamp = tsObj.as<uint64_t>();

			msgpack::unpacked typeUnpacked;
			std::size_t typeOffset = 0;
			msgpack::unpack(typeUnpacked, static_cast<const char*>(msg.data()) + totalOffset, msg.size() - totalOffset, typeOffset);
			totalOffset += typeOffset;
			msgpack::object typeObj = typeUnpacked.get();
			auto type = typeObj.as<std::string>();

			if (type != "image")
			{
				continue;
			}

			msgpack::unpacked formatUnpacked;
			std::size_t formatOffset = 0;
			msgpack::unpack(formatUnpacked, static_cast<const char*>(msg.data()) + totalOffset, msg.size() - totalOffset, formatOffset);
			totalOffset += formatOffset;
			msgpack::object formatObj = formatUnpacked.get();
			auto format = formatObj.as<std::string>();

			if (format != "RGBA")
			{
				continue;
			}

			msgpack::unpacked formatSizeUnpacked;
			std::size_t formatSizeOffset = 0;
			msgpack::unpack(formatSizeUnpacked, static_cast<const char*>(msg.data()) + totalOffset, msg.size() - totalOffset, formatSizeOffset);
			totalOffset += formatSizeOffset;
			msgpack::object formatSizeObj = formatSizeUnpacked.get();
			auto formatSize = formatSizeObj.as<std::array<int, 3>>();

			msgpack::unpacked metaUnpacked;
			std::size_t metaOffset = 0;
			msgpack::unpack(metaUnpacked, static_cast<const char*>(msg.data()) + totalOffset, msg.size() - totalOffset, metaOffset);
			totalOffset += metaOffset;
			msgpack::object metaObj = metaUnpacked.get();
			auto meta = metaObj.as<std::string>();



			AvCore::SDataPacketPtr ptrPacketJson;
			if (!m_connOutMeta.AllocatePacket(ptrPacketJson, meta.size()))
			{
				continue;
			}

			memcpy(ptrPacketJson.pDataBuffer, meta.data(), meta.size());
			m_connOutMeta.SetData(ptrPacketJson);


			std::size_t imgOffset = totalOffset;
			if (formatSize[0] * formatSize[1] * formatSize[2] / 8 < 256)
			{
				imgOffset += 2;
			}
			else if (formatSize[0] * formatSize[1] * formatSize[2] / 8 < 65536)
			{
				imgOffset += 3;
			}
			else
			{
				imgOffset += 5;
			}

			std::size_t imgSize = msg.size() - imgOffset;

			if (imgSize != (uint64_t)formatSize[0] * (uint64_t)formatSize[1] * (uint64_t)(formatSize[2]/8))
			{
				continue;
			}

			if (m_uiOutputWidth != formatSize[0] || m_uiOutputHeight != formatSize[1])
			{
				m_uiOutputWidth = formatSize[0];
				m_uiOutputHeight = formatSize[1];

				m_connOutImg.SetImageSize(m_uiOutputWidth, m_uiOutputHeight);
			}


			AvCore::SDataPacketPtr ptrPacket;
			if (!m_connOutImg.AllocatePacket(ptrPacket, imgSize))
			{
				continue;
			}

			memcpy(ptrPacket.pDataBuffer, static_cast<char*>(msg.data()) + imgOffset, imgSize);
			m_connOutImg.SetData(ptrPacket);

		}
		catch (zmq::error_t& e)
		{

		}
	}

	m_ZmqRecvSock.close();

	return 0;
}
