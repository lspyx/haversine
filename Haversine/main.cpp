#include <iostream>
#include <variant>
#include <list>
#include <map>
#include <memory>
#include <string>

namespace json {
  namespace detail {
    std::string tr_spaces_d_newlines_trim(const std::string &str) {
      std::string result;
      bool prev_was_space = false;
      for (const auto c : str) {
	if (c == ' ' || c == '\t') {
	  if (!prev_was_space) {
	    result += ' ';
	    prev_was_space = true;
	  }
	  continue;
	} else {
	  if (c != '\n') {
	    result += c;
	  } else {
	    prev_was_space = false;
	  }
	}
      }
      if (result[0] == ' ') result.erase(result.begin());
      if (result[result.size() - 1] == ' ') result.erase(result.end() - 1);
      return result;
    }
    
    std::string normalize(const std::string &str) {
      return tr_spaces_d_newlines_trim(str);
    }
  }
  
  using Key = std::string;

  struct Object;
  struct List;
  
  struct Value {
    std::variant<std::string, double, long, std::unique_ptr<List>, std::unique_ptr<Object>> value;
  };

  struct Object {
    std::map<Key, Value> value;
  };

  struct List {
    std::list<Value> value;
  };

  enum class eParseState {
    Null,
    Key,
    ValueObject,
    ValueString,
    ValueDouble,
    ValueLong,
    ValueList
  };

  template<eParseState>
  struct ReturnType {};

  template<> struct ReturnType<eParseState::Key> { using type = Key; };
  template<> struct ReturnType<eParseState::ValueObject> { using type = Object; };
  template<> struct ReturnType<eParseState::ValueString> { using type = std::string; };
  template<> struct ReturnType<eParseState::ValueDouble> { using type = double; };
  template<> struct ReturnType<eParseState::ValueLong> { using type = long; };
  template<> struct ReturnType<eParseState::ValueList> { using type = List; };
  
  template<eParseState state>
  ReturnType<state> &get_underlying_value(Value &value) {
    return std::get<ReturnType<state>>(value.value);
  };
  
  
  Object parse_json(std::string json) {
    Object root;
    root.value["root"];
    detail::normalize(json);
    Object *cur_obj = &root;
    Value intermediate_value;
    
    for (int i = 0; i < json.size(); i++) {
      const char c = json[i];
      switch (c) {
      case '{': // end object
      case '}': // start object
      case '"': // toggle string mode
      case '[': // start list
      case ']': // end list
      case ':': // switch to value collection
      default: // if string -> collect chars : else  -> collect double
	return root;
      }
    }
    return root;
  }
}

int main() {
  const std::string test_json = R"({
				  "key1" : { "subkey" : [ 123, 23, 3], "name" : "my-name", "value" : 2.2312 },
                                  "key2" :{"wants" : "gets"}
				  })";
    
  std::cout << json::detail::normalize(test_json) << std::endl;
}
