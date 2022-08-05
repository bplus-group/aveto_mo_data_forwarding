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

// Suppress the deprecated use of the codecvt library for C++17 and Visual Studio.
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

// Suppress the deprecated use of the filesystem library for c++14 and Visual Studio 2019.
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING

// Product information
#pragma once
#define VERSIONINFO_PRODUCT             "Data Forwarding MO"
#define VERSIONINFO_AUTHOR              "Christopher Maneth"
#define VERSIONINFO_COMMENTS            ""

// Version information
#define VERSIONINFO_VERSION             2.4
#define VERSIONINFO_BUILD               240
#define VERSIONINFO_SUBBUILD            0

// ZMQ configuration
#define ZEROMQ_LISTEN					"tcp://0.0.0.0:"
#define ZEROMQ_START_PORT				(5770)
#define ZEROMQ_CHANNEL_LIMIT			(20)
#define ZEROMQ_TOPIC					"out/image0"
#define ZEROMQ_SOURCE					"image0"

#if defined(_MSC_VER)
#	define NOMINMAX
#   include <windows.h>
#endif

#include <Core/AvCore.h>
#include <Dev/Support/AvProcessorObj.h>

#include <iostream>
#include <chrono>
#include <thread>
#include <queue>
#include <msgpack.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>


class CProcessorObject : public AVETO::Dev::Support::CAvetoProcessorObject
{
public:
	CProcessorObject();

	~CProcessorObject() = default;

	DECLARE_OBJECT_CLASS_NAME("Data Forwarding MO")
	DECLARE_OBJECT_GROUP_ASSOC(AVETO::Core::g_szGroupGeneric)

	// Connector map
	BEGIN_AVETO_CONNECTOR_MAP()
		AVETO_CONNECTOR_CYCLE_INPUT_FIRE_AND_FORGET("*", "Input Raw", ProcessData)
	END_AVETO_CONNECTOR_MAP()

	// Interface map
	BEGIN_AVETO_INTERFACE_MAP()
		AVETO_INTERFACE_CHAIN_BASE(AVETO::Dev::Support::CAvetoProcessorObject)
	END_AVETO_INTERFACE_MAP()

	// Property map
	BEGIN_AVETO_PROPERTY_MAP()
		AVETO_PROPERTY_CHAIN_BASE(AVETO::Dev::Support::CAvetoProcessorObject)
		AVETO_PROPERTY_ENTRY(m_uiFPSLimit, "FPS Limit", "The fps rate used for forwarding")
		AVETO_PROPERTY_ENTRY(m_bNoPayload, "No Payload", "If set only metadata will be sent")
		AVETO_PROPERTY_SET_READONLY_FLAG()
		AVETO_PROPERTY_ENTRY(m_iZmqChannel, "Forward Channel", "The channel used for forwarding")
		AVETO_PROPERTY_ENTRY(m_iZmqPort, "Forward Port", "The tcp port used for forwarding")
		AVETO_PROPERTY_RESET_READONLY_FLAG()
	END_AVETO_PROPERTY_MAP()

	/**
	 * \brief Override of IInitialize::Initialize.
	 *		Initialization function called when this object is created.
	 * \return Returns non-error status (e.g. AVETO_S_OK) if the initialization was successfull,
	 *		or an error status (e.g. AVETO_E_GENERIC_ERROR) if it failed.
	 */
	AVETO::Core::TStatus Initialize() override;

	/**
	 * \brief Override of IInitialize::Terminate.
	 *		Termination function called when this object is to be destroyed.
	 * \return Returns non-error status (e.g. AVETO_S_OK) if the termination was successfull,
	 *		or an error status (e.g. AVETO_E_GENERIC_ERROR) if it failed.
	 */
	AVETO::Core::TStatus Terminate() override;

	/**
	 * \brief Processes received data packets.
	 * \param[in] rgsPackets An array of data packets.
	 * \param[in] uiPackets The amount of packets in the received array.
	 * \attention The number of packets in the array can be 0; if so rgsPackets is a nullptr!
	 */
	void ProcessData(const AVETO::Core::SDataPacket* rgsPackets, uint32_t uiPackets);

	/**
	* \brief Reflected connector changed event (forwarded from one of the connectors). Overload of
	* AvCore::CAvetoMeasObject::OnConnectedConnectorChanged.
	*/
	virtual void OnConnectedConnectorChanged(const char* szOwnConnectorName, uint32_t uiOwnConnectorFlags,
		AVETO::Core::TObjID tConnectedConnectorID) override;

	/**
	 * \brief Called automatically when a connector establishes a connection
	 * \param[in] rsConnectInfo Struct that contains information about the connection
	 */
	virtual void OnConnect(const AVETO::Core::SConnectionEvent& rsConnectInfo) override;

private:
	std::queue<std::shared_ptr<AvCore::SDataPacketPtr>>			m_queuePackets;					//!< The image data in RGBA format
	std::mutex													m_mtxPacketQueue;				//!< Protect the packet queue.
	uint32_t													m_uiFPSLimit;
	bool														m_bNoPayload;

	std::string													m_ssInputName;
	uint32_t													m_uiInputWidth;
	uint32_t													m_uiInputHeight;
	uint32_t													m_uiInputBPP;
	bool														m_bInputIsRGBA;

	// ZeroMQ
	bool														m_bZeroMqActive{};
	std::thread													m_ZeroMQThread;					//!< ZeroMQ send thread
	std::mutex													m_mtxZeroMQMtx;					//!< ZeroMQ Mutex
	zmq::context_t												m_ZmqCtx;						//!< ZeroMQ context
	zmq::socket_t												m_ZmqSock;						//!< ZeroMQ bind socket
	msgpack::sbuffer											m_MsgBuffer;					//!< Msgpack message buffer
	std::atomic<bool>											m_bZeroMQActive;				//!< ZeroMQ loop active ?
	int															m_iZmqChannel;
	int															m_iZmqPort;


	int ZeroMQLoop();

	bool BuildMsgBuffer();

	void SendMsgBuffer();

	void HandleConnectorChange(AVETO::Core::TObjID tConnectedConnectorID);

	bool IsValidRGBA(AVETO::Core::TObjID tConnectedConnectorID) const;
};

DEFINE_AVETO_OBJECT(CProcessorObject)
