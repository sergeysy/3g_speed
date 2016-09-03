/*
 * SpeedMeasure.cpp
 *
 *  Created on: Oct 14, 2014
 *      Author: niccom
 */

#include "SpeedMeasure.h"
#include <assert.h>
#include <stdio.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <sys/time.h>

#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

#include <utility>
#include <sys/ioctl.h>
#include <net/if.h>
//#include <gps.h>

#include "Inih/INIReader.h"

using namespace std;

SpeedMeasure::SpeedMeasure(std::string& str) :
		mEnableMeasure(false)
{
	//printf("1\n");
	cout<<"ctor SpeedMeasure"<<endl;
	INIReader mIniReader(str);

	mOperatorName = mIniReader.Get("operator", "operator", "");
	mInterfaceName = mIniReader.Get("interface", "interface", "");
	mMeasurePeriod = mIniReader.GetInteger("measurePeriod", "measurePeriod", 1);
	mMagic = mIniReader.GetInteger("magic", "magic", 12345);

	mServerNameEstevano = mIniReader.Get("statistica", "statHost", "");

	mServerNameDownload = mIniReader.Get("downloadFile", "downloadServer", "");
	//printf("mServerNameDownload = %s\n", mServerNameDownload.c_str());
	cout<<"bla-bla-bla"<<mServerNameDownload<<" may be www.ya.ru"<<endl;

	mServerNameUpload = mIniReader.Get("uploadFile", "uploadServer", "");
	printf("mServerNameUpload = %s\n", mServerNameUpload.c_str());

	mDownloadFile = mIniReader.Get("downloadFile", "downloadFile", "");

	mUploadFile = mIniReader.Get("uploadFile", "uploadFile", "");

	mStatActionRegister = mIniReader.Get("statistica", "statActionRegister", "");
	mStatActionData = mIniReader.Get("statistica", "statActionData", "");
	mStatActionSession = mIniReader.Get("statistica", "statActionSession", "");
	mStatActionUpload = mIniReader.Get("statistica", "statActionUpload", "");

	std::string logFileName = mIniReader.Get("log", "log", "3gSpeedData.txt");

	mFileMeasureData.open(logFileName.c_str(), std::ios_base::app);

	registerDeviceOnServer();

	jsonInit(mJsonData);

    //std::pair<float, float> gpsD = getGpsData();

    //printf("Latitude=%f\n", gpsD.first);
    //printf("Longitude=%f\n", gpsD.second);
}

SpeedMeasure::~SpeedMeasure()
{
	mFileMeasureData.close();
}

void SpeedMeasure::startMeasure()
{
	mEnableMeasure = true;

	connectionInit();

	while (mEnableMeasure == true)
	{
		MeasureData measureData;

		struct timeval start, end;
		long seconds, useconds;

		mTcpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

		assert(mTcpSocket > 0);

		struct ifreq ifr;

		memset(&ifr, 0, sizeof(ifr));

		snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), mInterfaceName.c_str());

		if (setsockopt(mTcpSocket, SOL_SOCKET, SO_BINDTODEVICE, (void*) &ifr, sizeof(ifr)) < 0)
		{
			perror("Can't bind socket to iface");
		}

		memset(&start, 0, sizeof(struct timeval));
		memset(&end, 0, sizeof(struct timeval));

		gettimeofday(&start, NULL);

		if (tcpConnect())
		{
			gettimeofday(&end, NULL);

			seconds = end.tv_sec - start.tv_sec;
			useconds = end.tv_usec - start.tv_usec;

			/*timeout in mS */
			measureData.tcpConnectTimeout = ((seconds) * 1000 + useconds / 1000.0) + 0.5;
		}
		else
		{
			measureData.tcpConnectTimeout = 0;
		}

		struct timeval start_http, end_http;
		bool httpConnectFlag = false;

		memset(&start_http, 0, sizeof(struct timeval));
		memset(&end_http, 0, sizeof(struct timeval));

		gettimeofday(&start_http, NULL);

		if (httpConnect())
		{
			httpConnectFlag = true;
		}
		else
		{
			httpConnectFlag = false;
			measureData.httpConnectTimeout = 0;
		}

		memset(&start, 0, sizeof(struct timeval));
		memset(&end, 0, sizeof(struct timeval));

		gettimeofday(&start, NULL);

		long numberReceived = 0;

		numberReceived = downloadFile();

		if (numberReceived)
		{
			if (httpConnectFlag)
			{
				gettimeofday(&end_http, NULL);

				seconds = end_http.tv_sec - start_http.tv_sec;
				useconds = end_http.tv_usec - start_http.tv_usec;

				measureData.httpConnectTimeout = ((seconds) * 1000 + useconds / 1000.0) + 0.5;
			}

			gettimeofday(&end, NULL);

			seconds = useconds = 0;

			measureData.downloadSpeed = 0;

			seconds = end.tv_sec - start.tv_sec;
			useconds = end.tv_usec - start.tv_usec;

			/*download speed  in KBit */
			measureData.downloadSpeed = (numberReceived * sizeof(uint8_t)) / ((seconds * 1000) + useconds / 1000.0);
		}
		else
		{
			measureData.downloadSpeed = 0;
		}

		numberReceived = 0;

		printMeasureToFile(measureData);
		sendMeasureDataToServer(measureData);
		close(mTcpSocket);

		memset(&start, 0, sizeof(struct timeval));
		memset(&end, 0, sizeof(struct timeval));

		gettimeofday(&start, NULL);

		int numberSended = uploadFile();

		gettimeofday(&end, NULL);

		seconds = useconds = 0;
		measureData.uploadSpeed = 0;

		seconds = end.tv_sec - start.tv_sec;
		useconds = end.tv_usec - start.tv_usec;
		printf("seconds = %d\n", seconds);
		printf("useconds = %d\n", useconds);

		measureData.uploadSpeed = (numberSended * sizeof(uint8_t)) / ((seconds * 1000) + useconds / 1000.0);

		usleep(mMeasurePeriod * mS);
	}

}

void SpeedMeasure::stopMeasure()
{
	mEnableMeasure = false;
	close(mTcpSocket);
}

int SpeedMeasure::tcpConnect()
{

	if (connect(mTcpSocket, (struct sockaddr*) &mServer, sizeof(mServer)) < 0)
	{
		perror("tcpConnect");
		return -1;
	}
	return 1;
}

int SpeedMeasure::httpConnect()
{

	std::string mHttpRequest = "GET /";
	mHttpRequest.append(mDownloadFile);
	mHttpRequest.append(std::string(" HTTP/1.1\r\nHost: "));
	mHttpRequest.append(mServerNameDownload);
	mHttpRequest.append(std::string("\r\n\r\n"));

	ssize_t nByte = send(mTcpSocket, mHttpRequest.c_str(), mHttpRequest.length(), 0);

	if (nByte <= 0)
	{
		perror("httpConnect");
		return -1;
	}
	return 1;
}

int SpeedMeasure::downloadFile()
{
	char buffer[httpSizeDownload];
	long int recivedLen = 0;
	int numberReceived = 0;

	while ((recivedLen = recv(mTcpSocket, buffer, httpSizeDownload - 1, 0)) > 0)
	{
		buffer[recivedLen] = '\0';
		numberReceived += recivedLen;
	}
	printf("SpeedMeasure::downloadFile() numberReceived = %d\n", numberReceived);
	printf("buffer = %s\n", buffer);
	if (recivedLen < 0)
	{
		perror("downloadFile");
		return -1;
	}
	return numberReceived;
}

int SpeedMeasure::uploadFile()
{
	struct hostent *hostp = NULL;

	memset(&mServerUpload, 0, sizeof(mServerUpload));

	mServerUpload.sin_family = AF_INET;
	mServerUpload.sin_port = htons(httpPort);

	printf("mServerNameUpload.c_str() UploadFile = %s\n", mServerNameUpload.c_str());
	if ((mServerUpload.sin_addr.s_addr = inet_addr(mServerNameUpload.c_str())) == (unsigned long) INADDR_NONE)
	{
		hostp = gethostbyname(mServerNameUpload.c_str());

		if (hostp == NULL)
		{
			perror("uploadFile1");
			return -1;
		}

		memcpy(&mServerUpload.sin_addr, hostp->h_addr_list[0], sizeof(mServerUpload.sin_addr));
	}

	mTcpSocketUpload = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	assert(mTcpSocketUpload > 0);

	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), mInterfaceName.c_str());
	if (setsockopt(mTcpSocketUpload, SOL_SOCKET, SO_BINDTODEVICE, (void*) &ifr, sizeof(ifr)) < 0)
	{
		perror("Can't bind socket to iface");
	}
	if (connect(mTcpSocketUpload, (struct sockaddr*) &mServerUpload, sizeof(mServerUpload)) < 0)
	{
		perror("uploadFile connect:");
		close(mTcpSocketUpload);
		return -1;
	}

	std::ifstream rfc2616;
	rfc2616.open(mUploadFile.c_str());

	std::string tmp_string((std::istreambuf_iterator<char>(rfc2616)), std::istreambuf_iterator<char>());

	std::string httpUploadFile = "POST ";
	httpUploadFile.append(mStatActionUpload);
	httpUploadFile.append(" HTTP/1.1\r\nHost: ");
	httpUploadFile.append(mServerNameUpload);
	httpUploadFile.append(std::string("\r\nContent-Length: "));

	std::stringstream length;

	length << tmp_string.length();
	httpUploadFile.append(length.str());
	httpUploadFile.append(std::string("\r\n\r\n"));

	httpUploadFile.append(tmp_string);
	httpUploadFile.append(std::string("\r\n"));

	printf("httpUploadFile.length() = %d\n", httpUploadFile.length());

	ssize_t nByte = send(mTcpSocketUpload, httpUploadFile.c_str(), httpUploadFile.length(), 0);

	if (nByte <= 0)
	{
		perror(" uploadFile send");
		close(mTcpSocketUpload);
		rfc2616.close();
		return -1;
	}

	ssize_t nByteRcvd;

	char buffer[httpSizeDownload];

	if ((nByteRcvd = recv(mTcpSocketUpload, buffer, httpSizeDownload - 1, 0)) > 0)
	{
		buffer[nByteRcvd] = '\0';
	}

	if (nByteRcvd < 0)
	{
		perror("uploadFile send");
		close(mTcpSocketUpload);
		rfc2616.close();
		return -1;
	}

	std::string buf = buffer;

	std::size_t it = buf.find("200 OK");

	if (it == std::string::npos)
		std::cout << "Successeful upload file" << std::endl;
	else
		std::cout << "Upload file fails" << std::endl;

	close(mTcpSocketUpload);
	rfc2616.close();
	return nByte;
}

int SpeedMeasure::printMeasureToFile(MeasureData& measureData)
{
	mFileMeasureData << std::endl;
	mFileMeasureData << "*" << std::setw(4) << measureData.tcpConnectTimeout << "*";
	mFileMeasureData << " *" << std::setw(4) << measureData.httpConnectTimeout << "*";
	mFileMeasureData << " *" << std::setw(10) << measureData.downloadSpeed << "*";
	mFileMeasureData << " *" << std::setw(10) << measureData.uploadSpeed << "*" << std::endl;
	return 1;
}

int SpeedMeasure::connectionInit()
{
	memset(&mServer, 0, sizeof(mServer));

	mServer.sin_family = AF_INET;
	mServer.sin_port = htons(httpPort);

	struct hostent *hostp;

	if ((mServer.sin_addr.s_addr = inet_addr(mServerNameDownload.c_str())) == (unsigned long) INADDR_NONE)
	{
		hostp = gethostbyname(mServerNameDownload.c_str());

		printf("mServerNameDownload.c_str() = %s\n", mServerNameDownload.c_str());

		if (hostp == NULL)
		{
			perror("connectionInit1");
			close(mTcpSocket);
			return -1;
		}

		memcpy(&mServer.sin_addr, hostp->h_addr_list[0], sizeof(mServer.sin_addr));
	}

	memset(&mServerUpload, 0, sizeof(mServerUpload));

	mServerUpload.sin_family = AF_INET;
	mServerUpload.sin_port = htons(httpPort);

	memset(hostp, 0, sizeof(struct hostent));

	if ((mServerUpload.sin_addr.s_addr = inet_addr(mServerNameUpload.c_str())) == (unsigned long) INADDR_NONE)
	{
		printf("++++++++++++++++++++++++++++++++++++++++++++++++");
		printf("mServerNameUpload.c_str() = %s\n", mServerNameUpload.c_str());
		printf("!!!!!!!!!!!!!!!!!!!!!!!!");
		hostp = gethostbyname(mServerNameUpload.c_str());

		if (hostp == NULL)
		{
			perror("connectionInitUpload");
			close(mTcpSocket);
			return -1;
		}

		memcpy(&mServerUpload.sin_addr, hostp->h_addr_list[0], sizeof(mServerUpload.sin_addr));
	}

	return 1;
}

unsigned short SpeedMeasure::getHashMacAddr()
{
	return mMagic;
}

int SpeedMeasure::registerDeviceOnServer()
{

	memset(&mServer, 0, sizeof(mServer));

	mServer.sin_family = AF_INET;
	mServer.sin_port = htons(httpPort);

	struct hostent *hostp;

	if ((mServer.sin_addr.s_addr = inet_addr(mServerNameEstevano.c_str())) == (unsigned long) INADDR_NONE)
	{
		hostp = gethostbyname(mServerNameEstevano.c_str());

		if (hostp == NULL)
		{
			perror("register device init");
			close(mTcpSocket);
			return -1;
		}

		memcpy(&mServer.sin_addr, hostp->h_addr_list[0], sizeof(mServer.sin_addr));
	}

	int mTcpSocket = socket(AF_INET, SOCK_STREAM,
	IPPROTO_TCP);

	assert(mTcpSocket > 0);

	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));

	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), mInterfaceName.c_str());

	if (setsockopt(mTcpSocket, SOL_SOCKET, SO_BINDTODEVICE, (void*) &ifr, sizeof(ifr)) < 0)
	{
		perror("Can't bind socket to iface");
	}

	if (connect(mTcpSocket, (struct sockaddr*) &mServer, sizeof(mServer)) < 0)
	{
		perror("register device Connect");
		return -1;
	}

	std::stringstream ss;

	unsigned short x = getHashMacAddr();

	ss << x;

	std::string str = ss.str();

	std::string mHttpRequest = "GET ";

	mHttpRequest.append(mStatActionRegister);
	mHttpRequest.append(str);
	mHttpRequest.append(" HTTP/1.1\r\nHost: ");
	mHttpRequest.append(mServerNameEstevano);
	mHttpRequest.append("\r\n\r\n");

	ssize_t nByte = send(mTcpSocket, mHttpRequest.c_str(), mHttpRequest.length(), 0);

	if (nByte <= 0)
	{
		perror("httpConnect");
		close(mTcpSocket);
		return -1;
	}

	ssize_t nByteRcvd;

	char buffer[httpSizeDownload];

	if ((nByteRcvd = recv(mTcpSocket, buffer, httpSizeDownload - 1, 0)) > 0)
	{
		buffer[nByteRcvd] = '\0';
	}

	std::string buf = buffer;

	std::size_t it = buf.find("200 OK");

	if (it == std::string::npos)
		std::cout << "Cant register this device" << std::endl;
	else
		std::cout << "Success register this device" << std::endl;

	if (nByteRcvd < 0)
	{
		perror("SenReport send");
		close(mTcpSocket);
		return -1;
	}

	close(mTcpSocket);
	return 1;
}

int SpeedMeasure::sendMeasureDataToServer(MeasureData& m)
{
	memset(&mServerEstevano, 0, sizeof(mServerEstevano));

	mServerEstevano.sin_family = AF_INET;
	mServerEstevano.sin_port = htons(httpPort);

	struct hostent *hostp;

	if ((mServerEstevano.sin_addr.s_addr = inet_addr(mServerNameEstevano.c_str())) == (unsigned long) INADDR_NONE)
	{
		hostp = gethostbyname(mServerNameEstevano.c_str());

		if (hostp == NULL)
		{
			perror("register device init");
			return -1;
		}

		memcpy(&mServerEstevano.sin_addr, hostp->h_addr_list[0], sizeof(mServerEstevano.sin_addr));
	}

	int mTcpSocketEstevano = socket(AF_INET, SOCK_STREAM,
	IPPROTO_TCP);

	assert(mTcpSocketEstevano > 0);

	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), mInterfaceName.c_str());
	if (setsockopt(mTcpSocketEstevano, SOL_SOCKET, SO_BINDTODEVICE, (void*) &ifr, sizeof(ifr)) < 0)
	{
		perror("Can't bind socket to iface");
	}

	if (connect(mTcpSocketEstevano, (struct sockaddr*) &mServerEstevano, sizeof(mServerEstevano)) < 0)
	{
		perror("send measure data to server");
		return -1;
	}

	std::string mHttpRequest = "POST ";
	mHttpRequest.append(mStatActionData);
	mHttpRequest.append(std::string(" HTTP/1.1\r\nHost: "));
	mHttpRequest.append(mServerNameEstevano);
	mHttpRequest.append(std::string("\r\n"));
	mHttpRequest.append(std::string("Accept: application/jsonrequest\r\n"));

	mHttpRequest.append(std::string("Content-Type: application/json\r\n"));
	mHttpRequest.append(std::string("Content-Encoding: identity\r\n"));
	mHttpRequest.append(std::string("Content-Type: application/jsonrequest\r\n"));

	std::stringstream s1;
	s1 << m.downloadSpeed;
	mJsonData.downloadSpeed.second = s1.str();

    //std::pair<float, float> gpsData = getGpsData();

	std::stringstream s2;
    //s2 << gpsData.first;
	mJsonData.latitude.second = s2.str();

	std::stringstream s3;
    //s3 << gpsData.second;
	mJsonData.longitude.second = s3.str();

	std::stringstream s4;
	s4 << m.uploadSpeed;
	mJsonData.uploadSpeed.second = s4.str();
	printf(" getSessionId()  \n");
	mJsonData.sessionId.second = getSessionId();
	printf(" getSessionId()  \n");
	std::string data;

	data.append("{");

	data.append("\"");
	data.append(mJsonData.deviceId.first);
	data.append("\"");
	data.append(":");
	data.append("\"");
	data.append(mJsonData.deviceId.second);
	data.append("\"");

	data.append(", ");

	data.append("\"");
	data.append(mJsonData.sessionId.first);
	data.append("\"");
	data.append(":");
	data.append("\"");
	data.append(mJsonData.sessionId.second);
	data.append("\"");

	data.append(", ");

	data.append("\"");
	data.append(mJsonData.downloadSpeed.first);
	data.append("\"");
	data.append(":");
	data.append("\"");
	data.append(mJsonData.downloadSpeed.second);
	data.append("\"");

	data.append(", ");

	data.append("\"");
	data.append(mJsonData.uploadSpeed.first);
	data.append("\"");
	data.append(":");
	data.append("\"");
	data.append(mJsonData.uploadSpeed.second);
	data.append("\"");

	data.append(", ");

	data.append("\"");
	data.append(mJsonData.operators.first);
	data.append("\"");
	data.append(":");
	data.append("\"");
	data.append(mJsonData.operators.second);
	data.append("\"");

	data.append(", ");

	data.append("\"");
	data.append(mJsonData.latitude.first);
	data.append("\"");
	data.append(":");
	data.append("\"");

	data.append(mJsonData.latitude.second);
	data.append("\"");

	data.append(", ");

	data.append("\"");
	data.append(mJsonData.longitude.first);
	data.append("\"");
	data.append(":");
	data.append("\"");
	data.append(mJsonData.longitude.second);
	data.append("\"");

	data.append("}");

	std::string jsonLength = "Content-Length: ";

	std::stringstream length;
	length << data.length();
	std::string jsonString = length.str();
	jsonLength.append(jsonString);

	jsonLength.append(std::string("\r\n\r\n"));

	mHttpRequest.append(jsonLength);

	mHttpRequest.append(data);

	ssize_t nByte = send(mTcpSocketEstevano, mHttpRequest.c_str(), mHttpRequest.length(), 0);

	if (nByte <= 0)
	{
		perror("mTcpSocketEstevano");
		close(mTcpSocketEstevano);
		return -1;
	}

	ssize_t nByteRcvd;

	char buffer[httpSizeDownload];

	if ((nByteRcvd = recv(mTcpSocketEstevano, buffer, httpSizeDownload - 1, 0)) > 0)
	{
		buffer[nByteRcvd] = '\0';
	}

	std::string buf = buffer;

	std::cout << buf << std::endl;

	std::size_t it = buf.find("200 OK");

	if (it == std::string::npos)
		std::cout << "Cant send data" << std::endl;
	else
		std::cout << "Success send data" << std::endl;

	if (nByteRcvd < 0)
	{
		perror("mTcpSocketEstevano");
		close(mTcpSocketEstevano);
		return -1;
	}

	close(mTcpSocketEstevano);
	return 1;
}

int SpeedMeasure::jsonInit(Json& json)
{
	std::string deviceId = "deviceId";
	json.deviceId.first = deviceId;

	std::stringstream sss;

	unsigned short x = getHashMacAddr();
	sss << x;
	json.deviceId.second = sss.str();

	std::string sessionId = "sessionId";
	json.sessionId.first = sessionId;

	std::string downloadSpeed = "downloadSpeed";
	json.downloadSpeed.first = downloadSpeed;

	std::string uploadSpeed = "uploadSpeed";
	json.uploadSpeed.first = uploadSpeed;

	std::string operators = "operators";
	json.operators.first = operators;
	json.operators.second = mOperatorName;

	std::string latitude = "latitude";
	json.latitude.first = latitude;

	std::string longitude = "longitude";
	json.longitude.first = longitude;

	return 1;
}

/*std::pair<float, float> SpeedMeasure::getGpsData()
{
	gps_data_t *gpsdata;
	gpsdata = (struct gps_data_t*) malloc(sizeof(struct gps_data_t));

	if (gps_open("localhost", "2947", gpsdata) < 0)
	{
		fprintf(stderr, "Could not connect to GPSd\n");
		std::pair<float, float> ret(0, 0);
		free(gpsdata);
		return ret;
	}

	gps_stream(gpsdata, WATCH_ENABLE | WATCH_JSON, NULL);

	while (gpsdata->status == 0)
	{
		if (gps_waiting(gpsdata, 500))
		{
			if (gps_read(gpsdata) == -1)
			{
				fprintf(stderr, "GPSd Error\n");
				gps_stream(gpsdata, WATCH_DISABLE, NULL);
				gps_close(gpsdata);
				std::pair<float, float> ret(0, 0);
				free(gpsdata);

				return ret;
				break;
			}
			else
			{
				if (gpsdata->status > 0)
				{
					std::pair<float, float> ret(gpsdata->fix.latitude, gpsdata->fix.longitude);
					free(gpsdata);
					return ret;
				}
			}
		}
		else
		{
			gps_stream(gpsdata, WATCH_ENABLE | WATCH_JSON, NULL);
		}
		usleep(1000);
	}
	std::pair<float, float> ret(0, 0);
	free(gpsdata);
	return ret;
}
*/
std::string SpeedMeasure::getSessionId()
{

	struct sockaddr_in mServer;
	memset(&mServer, 0, sizeof(mServer));

	mServer.sin_family = AF_INET;
	mServer.sin_port = htons(httpPort);

	struct hostent *hostp;

	if ((mServer.sin_addr.s_addr = inet_addr(mServerNameEstevano.c_str())) == (unsigned long) INADDR_NONE)
	{
		hostp = gethostbyname(mServerNameEstevano.c_str());

		if (hostp == NULL)
		{
			perror("register device init");
			return std::string("");
		}

		memcpy(&mServer.sin_addr, hostp->h_addr_list[0], sizeof(mServer.sin_addr));
	}

	int mTcpSocket = socket(AF_INET, SOCK_STREAM,
	IPPROTO_TCP);

	assert(mTcpSocket > 0);

	if (connect(mTcpSocket, (struct sockaddr*) &mServer, sizeof(mServer)) < 0)
	{
		perror("register device Connect");
		return std::string("");
	}

	std::stringstream ss;
	unsigned short x = getHashMacAddr();

	ss << x;

	std::string str = ss.str();

	std::string mHttpRequest = "GET ";

	mHttpRequest.append(mStatActionSession);
	mHttpRequest.append(str);
	mHttpRequest.append(std::string(" HTTP/1.1\r\nHost: "));
	mHttpRequest.append(mServerNameEstevano);
	mHttpRequest.append(std::string("\r\n\r\n"));

	printf("mHttpRequest = %s\n", mHttpRequest.c_str());

	ssize_t nByte = send(mTcpSocket, mHttpRequest.c_str(), mHttpRequest.length(), 0);

	if (nByte <= 0)
	{
		perror("httpConnect");
		close(mTcpSocket);
		return std::string("");
	}

	ssize_t nByteRcvd;

	char buffer[httpSizeDownload];

	if ((nByteRcvd = recv(mTcpSocket, buffer, httpSizeDownload - 1, 0)) > 0)
	{
		buffer[nByteRcvd] = '\0';
	}

	std::string buf = buffer;

	std::size_t it = buf.find("SessionName>");

	if (!it)
	{
		perror("Bad request");
		close(mTcpSocket);
		return std::string("");
	}

	std::string stri = buf.substr(it, std::string::npos);
	std::size_t it1 = stri.find(">");
	std::size_t it2 = stri.find("<");

	it1++;

	int i = 0;
	char output[64];

	do
	{
		output[i++] = stri[it1++];
	} while (it1 != it2);

	output[i] = '\0';

	printf("SUB=%s\n", output);
	printf("***************************************************\n");
	printf("%s\n", buffer);
	printf("***************************************************\n");

	if (it == std::string::npos)
		std::cout << "Can't get sesion Id" << std::endl;
	else
		std::cout << "Success get sesion Id" << std::endl;

	if (nByteRcvd < 0)
	{
		perror("SenReport send");
		close(mTcpSocket);
		return std::string("");
	}

	close(mTcpSocket);
	std::string ret(output);
	return ret;
}

