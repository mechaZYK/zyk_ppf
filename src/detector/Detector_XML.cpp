#include "Detector_XML.h"
#include "Detectors_2.h"

bool Detector_XML::parseDetectorParams(TiXmlHandle hDoc, CDetectors3D::detectorParams &DetectorParams)
{
	try
	{
		char* seg = " ";

		TiXmlElement* rElem;
		rElem = hDoc.FirstChild("DetectParams3D").Element();
		if (rElem->ValueTStr() == "DetectParams3D")
		{
			TiXmlElement *detector;
			detector = rElem->FirstChild("DetectorParams")->ToElement();
			if (detector->ValueTStr() == "DetectorParams")
			{
				std::vector<std::string> parms_str = split(detector->GetText(), seg);
				DetectorParams.smoothKNN = atoi(parms_str[0].c_str());
				DetectorParams.downSampleRatio = atof(parms_str[1].c_str());
				DetectorParams.keypointerRatio = atof(parms_str[2].c_str());
				DetectorParams.matchMaxNum = atoi(parms_str[3].c_str());
			}
			else
			{
				return false;
			}
		}
		else
		{
			return false;
		}
		return true;
	}
	catch (...)
	{
		return false;
	}
}


bool Detector_XML::parseDetectObjParams(TiXmlHandle hDoc, vector <CDetectModel3D::detectObjParams> &DetectObjsParams)
{
	try
	{
		char* seg = " ";
		TiXmlElement* rElem;
		rElem = hDoc.FirstChild("DetectParams3D").Element();
		if (rElem->ValueTStr() == "DetectParams3D")
		{
			TiXmlElement *detector;
			detector = rElem->FirstChild("DetectObjectParams")->ToElement();
			if (detector->ValueTStr() == "DetectObjectParams")
			{
				for (TiXmlElement *dectectObj = detector->FirstChild()->ToElement(); dectectObj; dectectObj = dectectObj->NextSiblingElement())
				{
					CDetectModel3D::detectObjParams _temp;
					std::vector<std::string> parms_str;
					_temp.ObjectName = dectectObj->ValueTStr().c_str();
					parms_str = split(dectectObj->GetText(), seg);
					_temp.ShowColor = parms_str[0];
					_temp.minScore = atof(parms_str[1].c_str());
					_temp.mlsOrder = atoi(parms_str[2].c_str());
					_temp.isDetected = (bool)atoi(parms_str[3].c_str());
					_temp.MaxOverlapDistRel = atof(parms_str[4].c_str());
					DetectObjsParams.push_back(_temp);
				}
			}
			else
				return false;
		}
		else
		{
			return false;
		}
		return true;
	}
	catch (...)
	{
		return false;
	}
}

void Detector_XML::Write3DdetectSetingParams(const std::string &filename, const CDetectors3D::detectorParams &DetectorParams, const vector <CDetectModel3D::detectObjParams> &DetectObjsParams)
{
	char text[1000];
	char* seg = " ";
	TiXmlDocument doc;
	TiXmlDeclaration * decl = new TiXmlDeclaration("1.0", "", "");
	doc.LinkEndChild(decl);

	TiXmlElement * root_element = new TiXmlElement("DetectParams3D");
	doc.LinkEndChild(root_element);
	//识别器参数
	TiXmlElement *detector = new TiXmlElement("DetectorParams");
	root_element->LinkEndChild(detector);

	sprintf(text, "%d%s%f%s%f%s%d", DetectorParams.smoothKNN, seg, DetectorParams.downSampleRatio, seg, DetectorParams.keypointerRatio, seg, DetectorParams.matchMaxNum);
	TiXmlText *paramValue = new TiXmlText(text);
	detector->LinkEndChild(paramValue);

	//识别对象参数
	TiXmlElement *detectObjs = new TiXmlElement("DetectObjectParams");
	root_element->LinkEndChild(detectObjs);
	for (int i = 0; i < DetectObjsParams.size(); i++)
	{
		TiXmlElement *detectObject = new TiXmlElement(DetectObjsParams[i].ObjectName.c_str());
		detectObjs->LinkEndChild(detectObject);
		sprintf(text, "%s%s%f%s%d%s%d%s%f", DetectObjsParams[i].ShowColor.c_str(), seg, DetectObjsParams[i].minScore, seg, DetectObjsParams[i].mlsOrder, seg, DetectObjsParams[i].isDetected, seg, DetectObjsParams[i].MaxOverlapDistRel);
		TiXmlText *paramValue = new TiXmlText(text);
		detectObject->LinkEndChild(paramValue);
	}
	doc.SaveFile(filename.c_str());
}