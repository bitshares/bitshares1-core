
#include <iostream>

#include <fc/io/fstream.hpp>
#include <fc/io/raw.hpp>

#include "serial_tester.hpp"

template<typename T> void _print_hex_char(T& out, char c)
{
    static const char hex_digit[] = "0123456789abcdef";
    out << hex_digit[(c >> 4) & 0x0F];
    out << hex_digit[(c     ) & 0x0F];
    return;
}

template<typename T> void _print_hex_string(T& out, fc::string &s, int indent)
{
    if (s.length() == 0)
		return;

    int i;
    fc::string indent_str("");

    for(i=0;i<indent;i++)
		indent_str += ' ';

    i = 0;
    for(char &c : s)        // C++11 range loop
    {
        if (i == 0)
			out << indent_str;
		else
            out << ' ';
        _print_hex_char(out, c);
        i++;
        if (i == 0x10)
        {
            out << '\n';
            i = 0;
        }
    }
    return;
}

serial_tester::~serial_tester()
{
    this->process_results();
    this->print_results();
    return;
}

void serial_tester::print_hex_char(char c)
{
    _print_hex_char(std::cout, c);
    return;
}

void serial_tester::print_hex_string(fc::string& s, int indent)
{
    _print_hex_string(std::cout, s, indent);
    return;
}

void serial_tester::print_hex_char(fc::ostream& out, char c)
{
    _print_hex_char(out, c);
    return;
}

void serial_tester::print_hex_string(fc::ostream& out, fc::string& s, int indent)
{
    _print_hex_string(out, s, indent);
    return;
}

#define CASE_LETTER \
	case 'a': case 'b': case 'c': case 'd': case 'e': \
	case 'f': case 'g': case 'h': case 'i': case 'j': \
	case 'k': case 'l': case 'm': case 'n': case 'o': \
	case 'p': case 'q': case 'r': case 's': case 't': \
	case 'u': case 'v': case 'w': case 'x': case 'y': \
	case 'z':                                         \
	case 'A': case 'B': case 'C': case 'D': case 'E': \
	case 'F': case 'G': case 'H': case 'I': case 'J': \
	case 'K': case 'L': case 'M': case 'N': case 'O': \
	case 'P': case 'Q': case 'R': case 'S': case 'T': \
	case 'U': case 'V': case 'W': case 'X': case 'Y': \
	case 'Z'
	
#define CASE_DIGIT \
	case '0': case '1': case '2': case '3': case '4': \
	case '5': case '6': case '7': case '8': case '9'

void parse_serialization_tests(
    std::vector<std::pair<fc::string, fc::string>> &result,
    fc::istream &in
    )
{
    fc::string label("");
    fc::string value("");
    
    // line matches ^[A-Za-z_][A-Za-z0-9_]*:
    while(true)
    {
		read_next_line:
        try
        {
            char c = in.get();
            switch(c)
            {
				CASE_LETTER:
                case '_':
					if (label != "")
					{
						result.emplace_back(label, value);
						value = "";
					}
					FC_ASSERT(value == "");
					label = c;
					while(true)
					{
						c = in.get();
						switch(c)
						{
							CASE_LETTER:
							CASE_DIGIT:
							case '_':
								label += c;
								break;
							case ':':
								while(true)
								{
									c = in.get();
									switch(c)
									{
										case ' ': case '\t': continue;
										case '\r': case '\n': goto read_next_line;
										case '#':
											while(true)
											{
												c = in.get();
												switch(c)
												{
													case '\r': case '\n':
														goto read_next_line;
													default:
														continue;
												}
											}
										default:
											FC_THROW_EXCEPTION(fc::parse_error_exception, "expected: comment|whitespace|eol");
									}
								}
							default:
								FC_THROW_EXCEPTION(fc::parse_error_exception, "expected: label");
						}
					}
					break;
				case ' ':
				case '\t':
					// parse bytes
					while(true)
					{
						c = in.get();
						uint8_t v;
						switch(c)
						{
							case '0':           v = 0x00; goto ln;
							case '1':           v = 0x10; goto ln;
							case '2':           v = 0x20; goto ln;
							case '3':           v = 0x30; goto ln;
							case '4':           v = 0x40; goto ln;
							case '5':           v = 0x50; goto ln;
							case '6':           v = 0x60; goto ln;
							case '7':           v = 0x70; goto ln;
							case '8':           v = 0x80; goto ln;
							case '9':           v = 0x90; goto ln;
							case 'a': case 'A': v = 0xA0; goto ln;
							case 'b': case 'B': v = 0xB0; goto ln;
							case 'c': case 'C': v = 0xC0; goto ln;
							case 'd': case 'D': v = 0xD0; goto ln;
							case 'e': case 'E': v = 0xE0; goto ln;
							case 'f': case 'F': v = 0xF0; goto ln;
							case ' ': case '\t': continue;
							case '\r': case '\n': goto read_next_line;
							case '#':
								while(true)
								{
									c = in.get();
									switch(c)
									{
										case '\r': case '\n':
											goto read_next_line;
										default:
											continue;
									}
								}
							default:
								FC_THROW_EXCEPTION(fc::parse_error_exception, "expected: byte|comment|whitespace|eol");
						}
						
						ln:
						// get low nibble value
						c = in.get();
						switch(c)
						{
							case '0':           v += 0x00; break;
							case '1':           v += 0x01; break;
							case '2':           v += 0x02; break;
							case '3':           v += 0x03; break;
							case '4':           v += 0x04; break;
							case '5':           v += 0x05; break;
							case '6':           v += 0x06; break;
							case '7':           v += 0x07; break;
							case '8':           v += 0x08; break;
							case '9':           v += 0x09; break;
							case 'a': case 'A': v += 0x0A; break;
							case 'b': case 'B': v += 0x0B; break;
							case 'c': case 'C': v += 0x0C; break;
							case 'd': case 'D': v += 0x0D; break;
							case 'e': case 'E': v += 0x0E; break;
							case 'f': case 'F': v += 0x0F; break;
							default:
								FC_THROW_EXCEPTION(fc::parse_error_exception, "expected: byte");
						}
						if (label == "")
							FC_THROW_EXCEPTION(fc::parse_error_exception, "expected: label");
                        value += (char) v;
					}
				case '\r': case '\n':
					goto read_next_line;
				case '#':
					while(true)
					{
						c = in.get();
						switch(c)
						{
							case '\r': case '\n':
								goto read_next_line;
							default:
								continue;
						}
					}
				default:
					FC_THROW_EXCEPTION(fc::parse_error_exception, "unexpected character at beginning of line");
            }
        }
        catch(fc::eof_exception& e)
        {
            break;
        }
    }
    
    if (label != "")
        result.emplace_back(label, value);

    return;
}

void serial_tester::add_expected_results(fc::istream &infile)
{
	parse_serialization_tests(this->v_expected_results, infile);
    // TODO:  Don't go through whole thing when adding multiple files
    for( auto& kv : this->v_expected_results )
    {
        this->m_expected_results[kv.first] = kv.second;
        this->s_untested_cases.insert(kv.first);
    }
	return;
}

void serial_tester::add_test_result(fc::string& name, fc::string& value)
{
    fc::optional<fc::string> ev;
    auto it = this->m_expected_results.find(name);
    
    if (it != this->m_expected_results.end())
        ev = this->m_expected_results.at(name);
    
    this->v_test_results.emplace_back(
        name,
        ev,
        value
        );

    this->s_untested_cases.erase(name);

    return;
}

void serial_tester::process_results()
{
    for( auto& kv : this->v_expected_results )
    {
        fc::optional<fc::string> ev = kv.second;
        fc::optional<fc::string> av;
       
        if (this->s_untested_cases.count(kv.first) != 0)
            this->v_test_results.emplace_back(kv.first, ev, av);
    }
    return;
}

void serial_tester::print_results()
{
    int ok_count = 0;
    int fail_count = 0;
    int unknown_count = 0;
    int untested_count = 0;
    bool printed_unknown_banner = false;
    
    std::cout <<   "=========================\n"
              << this->test_name
              << "\n=========================\n";
    
    for( test_result& tr : this->v_test_results )
    {
        bool has_er = tr.expected_result.valid();
        bool has_ar = tr.actual_result.valid();
        
        if (has_er)
        {
            if (has_ar)
            {
                if (tr.actual_result == tr.expected_result)
                {
                    ok_count++;
                    continue;
                }
                fail_count++;
                std::cout << tr.name << ":                   # expected (failed)\n";
                this->print_hex_string(*tr.expected_result, 4);
                std::cout << '\n' << tr.name << ":                   # actual\n";
                this->print_hex_string(*tr.actual_result, 4);
                std::cout << '\n';
            }
            else
            {
                std::cout << tr.name << ":                   # expected (untested)\n";
                this->print_hex_string(*tr.expected_result, 4);
                untested_count++;
            }
        }
        else
        {
            FC_ASSERT(this->unknown_output);
            if (!printed_unknown_banner)
            {
                (*this->unknown_output) << "#########################\n"
                                        << "# " << this->test_name
                                        << "\n#########################\n\n";
                printed_unknown_banner = true;
            }
            FC_ASSERT(has_ar);
            (*this->unknown_output) << tr.name << ":\n";
            this->print_hex_string(*this->unknown_output, *tr.actual_result, 4);
            unknown_count++;
        }
    }
    std::cout <<   "ok      : " << ok_count
              << "\nfail    : " << fail_count
              << "\nunknown : " << unknown_count
              << "\nuntested: " << untested_count
              << '\n';
    return;
}
