/*
 * SpeedMeasure.h
 *
 *  Created on: Oct 14, 2014
 *      Author: niccom
 */

#ifndef SPEEDMEASURE_H_
#define SPEEDMEASURE_H_

#include <stdint.h>
#include <netinet/in.h>
#include <string>
#include <iostream>
#include <fstream>

typedef struct
{
	long tcpConnectTimeout;
	long httpConnectTimeout;
	long downloadSpeed;
	uint32_t uploadSpeed;
} MeasureData;

typedef struct
{
	std::pair<std::string, std::string> deviceId;
	std::pair<std::string, std::string> sessionId;
	std::pair<std::string, std::string> downloadSpeed;
	std::pair<std::string, std::string> uploadSpeed;
	std::pair<std::string, std::string> operators;
	std::pair<std::string, std::string> latitude;
	std::pair<std::string, std::string> longitude;
} Json;

class SpeedMeasure
{
	static const uint32_t httpPort = 80;
	static const uint32_t httpSizeDownload = 10000;
	static const uint32_t mS = 1000;

public:
	SpeedMeasure(std::string& str);
	virtual ~SpeedMeasure();
	void startMeasure();
	void stopMeasure();
private:
	int connectionInit();
	int tcpConnect();
	int httpConnect();
	int downloadFile();
	int uploadFile();
	int printMeasureToFile(MeasureData&);
	int sendMeasureDataToServer(MeasureData&);
	unsigned short getHashMacAddr();
	int registerDeviceOnServer();
	int jsonInit(Json&);
	std::pair<float, float> getGpsData();
	std::string getSessionId();
private:
	Json mJsonData;

	std::string mOperatorName;
	std::string mServerNameDownload;
	std::string mServerNameUpload;
	std::string mServerNameEstevano;
	std::string mInterfaceName;
	std::string mDownloadFile;
	std::string mUploadFile;

	std::string mStatActionRegister;
	std::string mStatActionData;
	std::string mStatActionSession;
	std::string mStatActionUpload;

	unsigned short mMagic;

	std::ofstream mFileMeasureData;

	MeasureData mMeasureData;
	uint32_t mMeasurePeriod;
	bool mEnableMeasure;

	int mTcpSocket;
	int mTcpSocketUpload;
	int mTcpSocketEstevano;

	struct sockaddr_in mServer;
	struct sockaddr_in mServerUpload;
	struct sockaddr_in mServerEstevano;
};

#endif /* SPEEDMEASURE_H_ */
