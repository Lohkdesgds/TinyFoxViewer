#pragma once

#include <string>
#include <vector>

class HttpInterpret {
	int status = 0;
	std::vector<std::pair<std::string, std::string>> m_headers;
	std::string m_body;
public:
	bool parse(const std::string&);

	std::string get_header(const std::string&);
	const std::vector<std::pair<std::string, std::string>>& get_headers() const;

	const std::string& get_body() const;

	bool valid() const;
	operator bool() const;
};