#pragma once
#include <WeaselCommon.h>
#include <windows.h>
#include <map>
#include <string>
#include <boost/function.hpp>
#include <boost/smart_ptr.hpp>


namespace weasel
{
	class Deserializer;

	// ����server�ؑ��ı�
	struct ResponseParser
	{
		std::map<std::wstring, boost::shared_ptr<Deserializer> > deserializers;

		std::wstring* p_commit;
		Context* p_context;
		Status* p_status;

		ResponseParser(std::wstring* commit, Context* context = 0, Status* status = 0);

		// ���d�����{���\���, �԰���ResponseHandler
		bool operator() (LPWSTR buffer, UINT length);

		// ̎��һ�лؑ��ı�
		void Feed(const std::wstring& line);
	};

}
