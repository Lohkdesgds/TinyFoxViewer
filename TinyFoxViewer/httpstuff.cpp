#include "httpstuff.h"

bool HttpInterpret::parse(const std::string& str)
{
	const static std::string _b4_status = "HTTP/1.1";
	const static std::string _b4_body = "\r\n\r\n";
	const static std::string _def_ln = "\r\n";

	size_t _assist = 0;

	if ((_assist = str.find(_b4_status)) == std::string::npos) return false;
	if ((_assist += _b4_status.size() + 1) > str.size()) return false;

	status = std::atoi(str.c_str() + _assist);

	_assist = str.find(_def_ln, _assist) + _def_ln.size();

	const size_t _lim = str.find(_b4_body);

	while (_assist < _lim) {
		const size_t _dot = str.find(":", _assist);
		if (_dot == std::string::npos) return false;

		std::pair<std::string, std::string> _pair;
		_pair.first = str.substr(_assist, _dot - _assist);

		_assist = str.find(_def_ln, _assist);

		_pair.second = str.substr(_dot + 1, _assist - (_dot + 1));
		_assist += _def_ln.size();

		while (_pair.second.size() && _pair.second.front() == ' ') _pair.second.erase(_pair.second.begin());

		m_headers.emplace_back(std::move(_pair));
	}
	
	if ((_assist = _lim + _b4_body.size()) > str.size()) return false;

	m_body = str.substr(_assist);

	return true;
}

std::string HttpInterpret::get_header(const std::string& src)
{
	for (const auto& i : m_headers) {
		if (i.first == src) return i.second;
	}
	return {};
}

const std::vector<std::pair<std::string, std::string>>& HttpInterpret::get_headers() const
{
	return m_headers;
}

const std::string& HttpInterpret::get_body() const
{
	return m_body;
}

bool HttpInterpret::valid() const
{
	return m_body.size() && m_headers.size();
}

HttpInterpret::operator bool() const
{
	return m_body.size() && m_headers.size();
}
