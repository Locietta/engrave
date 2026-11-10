/*
 * JSON schema validator for JSON for modern C++
 *
 * Copyright (c) 2016-2019 Patrick Boettcher <p@yai.se>.
 *
 * SPDX-License-Identifier: MIT
 *
 * Ported to glaze for engrave: namespace json_schema, json-dom.hpp's model.
 */
#include "json-schema.hpp"

#include "json-patch.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace json_schema;

#ifdef JSON_SCHEMA_BOOST_REGEX
#	include <boost/regex.hpp>
#	define REGEX_NAMESPACE boost
#elif defined(JSON_SCHEMA_NO_REGEX)
#	define NO_STD_REGEX
#else
#	include <regex>
#	define REGEX_NAMESPACE std
#endif

namespace
{

// nlohmann::json's value_t, in its order: the validator indexes its type table
// with it.  A JSON unsigned integer is number_unsigned, a negative one
// number_integer and any other number number_float.
enum class value_t : std::uint8_t {
	null,
	object,
	array,
	string,
	boolean,
	number_integer,
	number_unsigned,
	number_float,
	binary,
	discarded
};

value_t type_of(const json &j)
{
	if (j.is_object())
		return value_t::object;
	if (j.is_array())
		return value_t::array;
	if (j.is_string())
		return value_t::string;
	if (j.is_boolean())
		return value_t::boolean;
	if (j.holds<std::uint64_t>())
		return value_t::number_unsigned;
	if (j.holds<std::int64_t>())
		return value_t::number_integer;
	if (j.holds<double>())
		return value_t::number_float;
	return value_t::null;
}

// nlohmann's range-for over a json: array elements, object values, nothing
// for null and the value itself for any other type.
template <class J, class F>
void for_each_value(J &value, F &&f)
{
	if (value.is_array()) {
		for (auto &element : value.get_array())
			f(element);
	} else if (value.is_object()) {
		for (auto &member : value.get_object())
			f(member.second);
	} else if (!value.is_null()) {
		f(value);
	}
}

// nlohmann's items(): object members, array elements keyed by their index,
// nothing for null and the value itself keyed "" for any other type.
template <class J, class F>
void for_each_item(J &value, F &&f)
{
	if (value.is_object()) {
		for (auto &member : value.get_object())
			f(member.first, member.second);
	} else if (value.is_array()) {
		std::size_t index = 0;
		for (auto &element : value.get_array())
			f(std::to_string(index++), element);
	} else if (!value.is_null()) {
		f(std::string(), value);
	}
}

// json::get<std::vector<std::string>>()
std::vector<std::string> string_list(const json &value)
{
	if (!value.is_array())
		throw type_error("type must be array, but is " + std::string(type_name(value)));
	std::vector<std::string> result;
	for (const auto &element : value.get_array())
		result.push_back(element.get<std::string>());
	return result;
}

// json::erase(key) on an object
void erase_key(json &object, const std::string &key)
{
	object.get_object().erase(key);
}

class schema
{
protected:
	root_schema *root_;
	json default_value_ = nullptr;

protected:
	virtual std::shared_ptr<schema> make_for_default_(
	    std::shared_ptr<::schema> & /* sch */,
	    root_schema * /* root */,
	    std::vector<json_uri> & /* uris */,
	    json & /* default_value */) const
	{
		return nullptr;
	};

public:
	virtual ~schema() = default;

	schema(root_schema *root)
	    : root_(root) {}

	virtual void validate(const json_pointer &ptr, const json &instance, json_patch &patch, error_handler &e) const = 0;

	virtual const json &default_value(const json_pointer &, const json &, error_handler &) const
	{
		return default_value_;
	}

	void set_default_value(const json &v) { default_value_ = v; }

	static std::shared_ptr<schema> make(json &schema,
	                                    root_schema *root,
	                                    const std::vector<std::string> &key,
	                                    std::vector<json_uri> uris);
};

class schema_ref : public schema
{
	const std::string id_;
	std::weak_ptr<schema> target_;
	std::shared_ptr<schema> target_strong_; // for references to references keep also the shared_ptr because
	                                        // no one else might use it after resolving

	void validate(const json_pointer &ptr, const json &instance, json_patch &patch, error_handler &e) const final
	{
		auto target = target_.lock();

		if (target)
			target->validate(ptr, instance, patch, e);
		else
			e.error(ptr, instance, "unresolved or freed schema-reference " + id_);
	}

	const json &default_value(const json_pointer &ptr, const json &instance, error_handler &e) const override final
	{
		if (!default_value_.is_null())
			return default_value_;

		auto target = target_.lock();
		if (target)
			return target->default_value(ptr, instance, e);

		e.error(ptr, instance, "unresolved or freed schema-reference " + id_);

		return default_value_;
	}

protected:
	virtual std::shared_ptr<schema> make_for_default_(
	    std::shared_ptr<::schema> &sch,
	    root_schema *root,
	    std::vector<json_uri> &uris,
	    json &default_value) const override
	{
		// create a new reference schema using the original reference (which will be resolved later)
		// to store this overloaded default value #209
		auto result = std::make_shared<schema_ref>(uris[0].to_string(), root);
		result->set_target(sch, true);
		result->set_default_value(default_value);
		return result;
	};

public:
	schema_ref(const std::string &id, root_schema *root)
	    : schema(root), id_(id) {}

	const std::string &id() const { return id_; }

	void set_target(const std::shared_ptr<schema> &target, bool strong = false)
	{
		target_ = target;
		if (strong)
			target_strong_ = target;
	}
};

} // namespace

namespace json_schema
{

class root_schema
{
	schema_loader loader_;
	format_checker format_check_;
	content_checker content_check_;

	std::shared_ptr<schema> root_;

	struct schema_file {
		std::map<std::string, std::shared_ptr<schema>> schemas;
		std::map<std::string, std::shared_ptr<schema_ref>> unresolved; // contains all unresolved references from any other file seen during parsing
		json unknown_keywords;
	};

	// location as key
	std::map<std::string, schema_file> files_;

	schema_file &get_or_create_file(const std::string &loc)
	{
		auto file = files_.lower_bound(loc);
		if (file != files_.end() && !(files_.key_comp()(loc, file->first)))
			return file->second;
		else
			return files_.insert(file, {loc, {}})->second;
	}

public:
	root_schema(schema_loader &&loader,
	            format_checker &&format,
	            content_checker &&content)

	    : loader_(std::move(loader)),
	      format_check_(std::move(format)),
	      content_check_(std::move(content))
	{
	}

	format_checker &format_check() { return format_check_; }
	content_checker &content_check() { return content_check_; }

	void insert(const json_uri &uri, const std::shared_ptr<schema> &s)
	{
		auto &file = get_or_create_file(uri.location());
		auto sch = file.schemas.lower_bound(uri.fragment());
		if (sch != file.schemas.end() && !(file.schemas.key_comp()(uri.fragment(), sch->first))) {
			throw std::invalid_argument("schema with " + uri.to_string() + " already inserted");
			return;
		}

		file.schemas.insert({uri.fragment(), s});

		// was someone referencing this newly inserted schema?
		auto unresolved = file.unresolved.find(uri.fragment());
		if (unresolved != file.unresolved.end()) {
			unresolved->second->set_target(s);
			file.unresolved.erase(unresolved);
		}
	}

	void insert_unknown_keyword(const json_uri &uri, const std::string &key, json &value)
	{
		auto &file = get_or_create_file(uri.location());
		auto new_uri = uri.append(key);
		auto fragment = new_uri.pointer();

		// is there a reference looking for this unknown-keyword, which is thus no longer a unknown keyword but a schema
		auto unresolved = file.unresolved.find(fragment.to_string());
		if (unresolved != file.unresolved.end())
			schema::make(value, this, {}, {{new_uri}});
		else { // no, nothing ref'd it, keep for later

			// need to create an object for each reference-token in the
			// JSON-Pointer When not existing, a stringified integer reference
			// token (e.g. "123") in the middle of the pointer will be
			// interpreted a an array-index and an array will be created.

			// json_pointer's reference_tokens is private - get them
			std::deque<std::string> ref_tokens;
			auto uri_pointer = uri.pointer();
			while (!uri_pointer.empty()) {
				ref_tokens.push_front(uri_pointer.back());
				uri_pointer.pop_back();
			}

			// for each token create an object, if not already existing
			auto unk_kw = &file.unknown_keywords;
			for (auto &rt : ref_tokens) {
				// create a json_pointer from rt as rt can be an stringified integer doing find on an array won't work
				json_pointer rt_ptr{"/" + rt};
				if (json_schema::contains(*unk_kw, rt_ptr) == false)
					(*unk_kw)[rt] = json::object_t{};
				unk_kw = &json_schema::get_or_create(*unk_kw, rt_ptr);
			}
			(*unk_kw)[key] = value;
		}

		// recursively add possible subschemas of unknown keywords
		if (type_of(value) == value_t::object)
			for_each_item(value, [&](const std::string &subkey, json &subvalue) {
				insert_unknown_keyword(new_uri, subkey, subvalue);
			});
	}

	std::shared_ptr<schema> get_or_create_ref(const json_uri &uri)
	{
		auto &file = get_or_create_file(uri.location());

		// existing schema
		auto sch = file.schemas.find(uri.fragment());
		if (sch != file.schemas.end())
			return sch->second;

		// referencing an unknown keyword, turn it into schema
		//
		// an unknown keyword can only be referenced by a json-pointer,
		// not by a plain name fragment
		if (!uri.pointer().to_string().empty()) {
			bool contains_pointer = json_schema::contains(file.unknown_keywords, uri.pointer());
			if (contains_pointer) {
				auto &subschema = json_schema::at(file.unknown_keywords, uri.pointer());
				auto s = schema::make(subschema, this, {}, {{uri}});
				if (s) { // if schema is valid (non-null)
					erase_key(file.unknown_keywords, uri.fragment());
					return s;
				}
			}
		}

		// get or create a schema_ref
		auto r = file.unresolved.lower_bound(uri.fragment());
		if (r != file.unresolved.end() && !(file.unresolved.key_comp()(uri.fragment(), r->first))) {
			return r->second; // unresolved, already seen previously - use existing reference
		} else {
			return file.unresolved.insert(r,
			                              {uri.fragment(), std::make_shared<schema_ref>(uri.to_string(), this)})
			    ->second; // unresolved, create reference
		}
	}

	void set_root_schema(json sch)
	{
		files_.clear();
		root_ = schema::make(sch, this, {}, {{"#"}});

		// load all files which have not yet been loaded
		do {
			bool new_schema_loaded = false;

			// files_ is modified during parsing, iterators are invalidated
			std::vector<std::string> locations;
			for (auto &file : files_)
				locations.push_back(file.first);

			for (auto &loc : locations) {
				if (files_[loc].schemas.size() == 0) { // nothing has been loaded for this file
					if (loader_) {
						json loaded_schema;

						loader_(loc, loaded_schema);

						schema::make(loaded_schema, this, {}, {{loc}});
						new_schema_loaded = true;
					} else {
						throw std::invalid_argument("external schema reference '" + loc + "' needs loading, but no loader callback given");
					}
				}
			}

			if (!new_schema_loaded) // if no new schema loaded, no need to try again
				break;
		} while (1);

		for (const auto &file : files_) {
			if (file.second.unresolved.size() != 0) {
				// Build a representation of the undefined
				// references as a list of comma-separated strings.
				auto n_urefs = file.second.unresolved.size();
				std::string urefs = "[";

				decltype(n_urefs) counter = 0;
				for (const auto &p : file.second.unresolved) {
					urefs += p.first;

					if (counter != n_urefs - 1u) {
						urefs += ", ";
					}

					++counter;
				}

				urefs += "]";

				throw std::invalid_argument("after all files have been parsed, '" +
				                            (file.first == "" ? "<root>" : file.first) +
				                            "' has still the following undefined references: " + urefs);
			}
		}
	}

	void validate(const json_pointer &ptr,
	              const json &instance,
	              json_patch &patch,
	              error_handler &e,
	              const json_uri &initial) const
	{
		if (!root_) {
			e.error(ptr, json(std::string()), "no root schema has yet been set for validating an instance");
			return;
		}

		auto file_entry = files_.find(initial.location());
		if (file_entry == files_.end()) {
			e.error(ptr, json(std::string()), "no file found serving requested root-URI. " + initial.location());
			return;
		}

		auto &file = file_entry->second;
		auto sch = file.schemas.find(initial.fragment());
		if (sch == file.schemas.end()) {
			e.error(ptr, json(std::string()), "no schema find for request initial URI: " + initial.to_string());
			return;
		}

		sch->second->validate(ptr, instance, patch, e);
	}
};

} // namespace json_schema

namespace
{

class first_error_handler : public error_handler
{
public:
	bool error_{false};
	json_pointer ptr_;
	json instance_;
	std::string message_;

	void error(const json_pointer &ptr, const json &instance, const std::string &message) override
	{
		if (*this)
			return;
		error_ = true;
		ptr_ = ptr;
		instance_ = instance;
		message_ = message;
	}

	operator bool() const { return error_; }
};

class logical_not : public schema
{
	std::shared_ptr<schema> subschema_;

	void validate(const json_pointer &ptr, const json &instance, json_patch &patch, error_handler &e) const final
	{
		first_error_handler esub;
		subschema_->validate(ptr, instance, patch, esub);

		if (!esub)
			e.error(ptr, instance, "the subschema has succeeded, but it is required to not validate");
	}

	const json &default_value(const json_pointer &ptr, const json &instance, error_handler &e) const override
	{
		return subschema_->default_value(ptr, instance, e);
	}

public:
	logical_not(json &sch,
	            root_schema *root,
	            const std::vector<json_uri> &uris)
	    : schema(root)
	{
		subschema_ = schema::make(sch, root, {"not"}, uris);
	}
};

enum logical_combination_types {
	allOf,
	anyOf,
	oneOf
};

class logical_combination_error_handler : public error_handler
{
public:
	struct error_entry {
		json_pointer ptr_;
		json instance_;
		std::string message_;
	};

	std::vector<error_entry> error_entry_list_;

	void error(const json_pointer &ptr, const json &instance, const std::string &message) override
	{
		error_entry_list_.push_back(error_entry{ptr, instance, message});
	}

	void propagate(error_handler &e, const std::string &prefix) const
	{
		for (const error_entry &entry : error_entry_list_)
			e.error(entry.ptr_, entry.instance_, prefix + entry.message_);
	}

	operator bool() const { return !error_entry_list_.empty(); }
};

template <enum logical_combination_types combine_logic>
class logical_combination : public schema
{
	std::vector<std::shared_ptr<schema>> subschemata_;

	void validate(const json_pointer &ptr, const json &instance, json_patch &patch, error_handler &e) const final
	{
		size_t count = 0;
		logical_combination_error_handler error_summary;

		for (std::size_t index = 0; index < subschemata_.size(); ++index) {
			const std::shared_ptr<schema> &s = subschemata_[index];
			logical_combination_error_handler esub;
			auto oldPatchSize = patch.get_json().size();
			s->validate(ptr, instance, patch, esub);
			if (!esub)
				count++;
			else {
				patch.get_json().get_array().resize(oldPatchSize);
				esub.propagate(error_summary, "case#" + std::to_string(index) + "] ");
			}

			if (is_validate_complete(instance, ptr, e, esub, count, index))
				return;
		}

		if (count == 0) {
			e.error(ptr, instance, "no subschema has succeeded, but one of them is required to validate. Type: " + key + ", number of failed subschemas: " + std::to_string(subschemata_.size()));
			error_summary.propagate(e, "[combination: " + key + " / ");
		}
	}

	// specialized for each of the logical_combination_types
	static const std::string key;
	static bool is_validate_complete(const json &, const json_pointer &, error_handler &, const logical_combination_error_handler &, size_t, size_t);

public:
	logical_combination(json &sch,
	                    root_schema *root,
	                    const std::vector<json_uri> &uris)
	    : schema(root)
	{
		size_t c = 0;
		for_each_value(sch, [&](json &subschema) {
			subschemata_.push_back(schema::make(subschema, root, {key, std::to_string(c++)}, uris));
		});

		// value of allOf, anyOf, and oneOf "MUST be a non-empty array"
		// TODO error/throw? when subschemata_.empty()
	}
};

template <>
const std::string logical_combination<allOf>::key = "allOf";
template <>
const std::string logical_combination<anyOf>::key = "anyOf";
template <>
const std::string logical_combination<oneOf>::key = "oneOf";

template <>
bool logical_combination<allOf>::is_validate_complete(const json &, const json_pointer &, error_handler &e, const logical_combination_error_handler &esub, size_t, size_t current_schema_index)
{
	if (esub) {
		e.error(esub.error_entry_list_.front().ptr_, esub.error_entry_list_.front().instance_, "at least one subschema has failed, but all of them are required to validate - " + esub.error_entry_list_.front().message_);
		esub.propagate(e, "[combination: allOf / case#" + std::to_string(current_schema_index) + "] ");
	}
	return esub;
}

template <>
bool logical_combination<anyOf>::is_validate_complete(const json &, const json_pointer &, error_handler &, const logical_combination_error_handler &, size_t count, size_t)
{
	return count == 1;
}

template <>
bool logical_combination<oneOf>::is_validate_complete(const json &instance, const json_pointer &ptr, error_handler &e, const logical_combination_error_handler &, size_t count, size_t)
{
	if (count > 1)
		e.error(ptr, instance, "more than one subschema has succeeded, but exactly one of them is required to validate");
	return count > 1;
}

class type_schema : public schema
{
	std::vector<std::shared_ptr<schema>> type_;
	std::pair<bool, json> enum_, const_;
	std::vector<std::shared_ptr<schema>> logic_;

	static std::shared_ptr<schema> make(json &schema,
	                                    value_t type,
	                                    root_schema *,
	                                    const std::vector<json_uri> &,
	                                    std::set<std::string> &);

	std::shared_ptr<schema> if_, then_, else_;

	void validate(const json_pointer &ptr, const json &instance, json_patch &patch, error_handler &e) const override final
	{
		// depending on the type of instance run the type specific validator - if present
		auto type = type_[static_cast<uint8_t>(type_of(instance))];

		if (type)
			type->validate(ptr, instance, patch, e);
		else
			e.error(ptr, instance, "unexpected instance type");

		if (enum_.first) {
			bool seen_in_enum = false;
			for_each_value(enum_.second, [&](const json &v) {
				if (!seen_in_enum && json_schema::equal(instance, v))
					seen_in_enum = true;
			});

			if (!seen_in_enum)
				e.error(ptr, instance, "instance not found in required enum");
		}

		if (const_.first &&
		    !json_schema::equal(const_.second, instance))
			e.error(ptr, instance, "instance not const");

		for (auto l : logic_)
			l->validate(ptr, instance, patch, e);

		if (if_) {
			first_error_handler err;

			if_->validate(ptr, instance, patch, err);
			if (!err) {
				if (then_)
					then_->validate(ptr, instance, patch, e);
			} else {
				if (else_)
					else_->validate(ptr, instance, patch, e);
			}
		}
		if (instance.is_null()) {
			patch.add(json_pointer{}, default_value_);
		}
	}

protected:
	virtual std::shared_ptr<schema> make_for_default_(
	    std::shared_ptr<::schema> & /* sch */,
	    root_schema * /* root */,
	    std::vector<json_uri> & /* uris */,
	    json &default_value) const override
	{
		auto result = std::make_shared<type_schema>(*this);
		result->set_default_value(default_value);
		return result;
	};

public:
	type_schema(json &sch,
	            root_schema *root,
	            const std::vector<json_uri> &uris)
	    : schema(root), type_(static_cast<uint8_t>(value_t::discarded) + 1)
	{
		// association between JSON-schema-type and NLohmann-types
		static const std::vector<std::pair<std::string, value_t>> schema_types = {
		    {"null", value_t::null},
		    {"object", value_t::object},
		    {"array", value_t::array},
		    {"string", value_t::string},
		    {"boolean", value_t::boolean},
		    {"integer", value_t::number_integer},
		    {"number", value_t::number_float},
		};

		std::set<std::string> known_keywords;

		auto &object = sch.get_object();
		auto attr = object.find("type");
		if (attr == object.end()) // no type field means all sub-types possible
			for (auto &t : schema_types)
				type_[static_cast<uint8_t>(t.second)] = type_schema::make(sch, t.second, root, uris, known_keywords);
		else {
			switch (type_of(attr->second)) { // "type": "type"

			case value_t::string: {
				auto schema_type = attr->second.get<std::string>();
				for (auto &t : schema_types)
					if (t.first == schema_type)
						type_[static_cast<uint8_t>(t.second)] = type_schema::make(sch, t.second, root, uris, known_keywords);
			} break;

			case value_t::array: // "type": ["type1", "type2"]
				for (auto &array_value : attr->second.get_array()) {
					auto schema_type = array_value.get<std::string>();
					for (auto &t : schema_types)
						if (t.first == schema_type)
							type_[static_cast<uint8_t>(t.second)] = type_schema::make(sch, t.second, root, uris, known_keywords);
				}
				break;

			default:
				break;
			}

			object.erase(attr);
		}

		attr = object.find("default");
		if (attr != object.end()) {
			set_default_value(attr->second);
			object.erase(attr);
		}

		for (auto &key : known_keywords)
			object.erase(key);

		// with nlohmann::json float instance (but number in schema-definition) can be seen as unsigned or integer -
		// reuse the number-validator for integer values as well, if they have not been specified explicitly
		if (type_[static_cast<uint8_t>(value_t::number_float)] && !type_[static_cast<uint8_t>(value_t::number_integer)])
			type_[static_cast<uint8_t>(value_t::number_integer)] = type_[static_cast<uint8_t>(value_t::number_float)];

		// #54: JSON-schema does not differentiate between unsigned and signed integer - nlohmann::json does
		// we stick with JSON-schema: use the integer-validator if instance-value is unsigned
		type_[static_cast<uint8_t>(value_t::number_unsigned)] = type_[static_cast<uint8_t>(value_t::number_integer)];

		// special for binary types
		if (type_[static_cast<uint8_t>(value_t::string)]) {
			type_[static_cast<uint8_t>(value_t::binary)] = type_[static_cast<uint8_t>(value_t::string)];
		}

		attr = object.find("enum");
		if (attr != object.end()) {
			enum_ = {true, attr->second};
			object.erase(attr);
		}

		attr = object.find("const");
		if (attr != object.end()) {
			const_ = {true, attr->second};
			object.erase(attr);
		}

		attr = object.find("not");
		if (attr != object.end()) {
			logic_.push_back(std::make_shared<logical_not>(attr->second, root, uris));
			object.erase(attr);
		}

		attr = object.find("allOf");
		if (attr != object.end()) {
			logic_.push_back(std::make_shared<logical_combination<allOf>>(attr->second, root, uris));
			object.erase(attr);
		}

		attr = object.find("anyOf");
		if (attr != object.end()) {
			logic_.push_back(std::make_shared<logical_combination<anyOf>>(attr->second, root, uris));
			object.erase(attr);
		}

		attr = object.find("oneOf");
		if (attr != object.end()) {
			logic_.push_back(std::make_shared<logical_combination<oneOf>>(attr->second, root, uris));
			object.erase(attr);
		}

		attr = object.find("if");
		if (attr != object.end()) {
			auto attr_then = object.find("then");
			auto attr_else = object.find("else");

			if (attr_then != object.end() || attr_else != object.end()) {
				if_ = schema::make(attr->second, root, {"if"}, uris);

				if (attr_then != object.end()) {
					then_ = schema::make(attr_then->second, root, {"then"}, uris);
					object.erase(attr_then);
				}

				if (attr_else != object.end()) {
					else_ = schema::make(attr_else->second, root, {"else"}, uris);
					object.erase(attr_else);
				}
			}
			object.erase(attr);
		}
	}
};

class string : public schema
{
	std::pair<bool, size_t> maxLength_{false, 0};
	std::pair<bool, size_t> minLength_{false, 0};

#ifndef NO_STD_REGEX
	std::pair<bool, REGEX_NAMESPACE::regex> pattern_{false, REGEX_NAMESPACE::regex()};
	std::string patternString_;
#endif

	std::pair<bool, std::string> format_;
	std::tuple<bool, std::string, std::string> content_{false, "", ""};

	std::size_t utf8_length(const std::string &s) const
	{
		size_t len = 0;
		for (auto c : s)
			if ((c & 0xc0) != 0x80)
				len++;
		return len;
	}

	void validate(const json_pointer &ptr, const json &instance, json_patch &, error_handler &e) const override
	{
		if (minLength_.first) {
			if (utf8_length(instance.get<std::string>()) < minLength_.second) {
				std::ostringstream s;
				s << "instance is too short as per minLength:" << minLength_.second;
				e.error(ptr, instance, s.str());
			}
		}

		if (maxLength_.first) {
			if (utf8_length(instance.get<std::string>()) > maxLength_.second) {
				std::ostringstream s;
				s << "instance is too long as per maxLength: " << maxLength_.second;
				e.error(ptr, instance, s.str());
			}
		}

		if (std::get<0>(content_)) {
			if (root_->content_check() == nullptr)
				e.error(ptr, instance, std::string("a content checker was not provided but a contentEncoding or contentMediaType for this string have been present: '") + std::get<1>(content_) + "' '" + std::get<2>(content_) + "'");
			else {
				try {
					root_->content_check()(std::get<1>(content_), std::get<2>(content_), instance);
				} catch (const std::exception &ex) {
					e.error(ptr, instance, std::string("content-checking failed: ") + ex.what());
				}
			}
		} else if (type_of(instance) == value_t::binary) {
			e.error(ptr, instance, "expected string, but get binary data");
		}

		if (type_of(instance) != value_t::string) {
			return; // next checks only for strings
		}

#ifndef NO_STD_REGEX
		if (pattern_.first &&
		    !REGEX_NAMESPACE::regex_search(instance.get<std::string>(), pattern_.second))
			e.error(ptr, instance, "instance does not match regex pattern: " + patternString_);
#endif

		if (format_.first) {
			if (root_->format_check() == nullptr)
				e.error(ptr, instance, std::string("a format checker was not provided but a format keyword for this string is present: ") + format_.second);
			else {
				try {
					root_->format_check()(format_.second, instance.get<std::string>());
				} catch (const std::exception &ex) {
					e.error(ptr, instance, std::string("format-checking failed: ") + ex.what());
				}
			}
		}
	}

public:
	string(json &sch, root_schema *root)
	    : schema(root)
	{
		auto &object = sch.get_object();
		auto attr = object.find("maxLength");
		if (attr != object.end()) {
			maxLength_ = {true, attr->second.as<size_t>()};
			object.erase(attr);
		}

		attr = object.find("minLength");
		if (attr != object.end()) {
			minLength_ = {true, attr->second.as<size_t>()};
			object.erase(attr);
		}

		attr = object.find("contentEncoding");
		if (attr != object.end()) {
			std::get<0>(content_) = true;
			std::get<1>(content_) = attr->second.get<std::string>();

			// special case for nlohmann::json-binary-types
			//
			// https://github.com/pboettch/json-schema-validator/pull/114
			//
			// We cannot use explicitly in a schema: {"type": "binary"} or
			// "type": ["binary", "number"] we have to be implicit. For a
			// schema where "contentEncoding" is set to "binary", an instance
			// of type json::value_t::binary is accepted. If a
			// contentEncoding-callback has to be provided and is called
			// accordingly. For encoding=binary, no other type validations are done

			object.erase(attr);
		}

		attr = object.find("contentMediaType");
		if (attr != object.end()) {
			std::get<0>(content_) = true;
			std::get<2>(content_) = attr->second.get<std::string>();

			object.erase(attr);
		}

		if (std::get<0>(content_) == true && root_->content_check() == nullptr) {
			throw std::invalid_argument{"schema contains contentEncoding/contentMediaType but content checker was not set"};
		}

#ifndef NO_STD_REGEX
		attr = object.find("pattern");
		if (attr != object.end()) {
			patternString_ = attr->second.get<std::string>();
			pattern_ = {true, REGEX_NAMESPACE::regex(attr->second.get<std::string>(),
			                                         REGEX_NAMESPACE::regex::ECMAScript)};
			object.erase(attr);
		}
#endif

		attr = object.find("format");
		if (attr != object.end()) {
			if (root_->format_check() == nullptr)
				throw std::invalid_argument{"a format checker was not provided but a format keyword for this string is present: " + format_.second};

			format_ = {true, attr->second.get<std::string>()};
			object.erase(attr);
		}
	}
};

template <typename T>
class numeric : public schema
{
	std::pair<bool, T> maximum_{false, 0};
	std::pair<bool, T> minimum_{false, 0};

	bool exclusiveMaximum_ = false;
	bool exclusiveMinimum_ = false;

	std::pair<bool, double> multipleOf_{false, 0};

	// multipleOf - if the remainder of the division is 0 -> OK
	bool violates_multiple_of(T x) const
	{
		double res = std::remainder(static_cast<double>(x), multipleOf_.second);
		double multiple = std::fabs(static_cast<double>(x) / multipleOf_.second);
		if (multiple > 1) {
			res = res / multiple;
		}
		double eps = std::nextafter(static_cast<double>(x), 0.0) - static_cast<double>(x);

		return std::fabs(res) > std::fabs(eps);
	}

	void validate(const json_pointer &ptr, const json &instance, json_patch &, error_handler &e) const override
	{
		T value = instance.as<T>(); // conversion of json to value_type

		std::ostringstream oss;

		if (multipleOf_.first && value != 0) // zero is multiple of everything
			if (violates_multiple_of(value))
				oss << "instance is not a multiple of " << json_schema::dump(json(multipleOf_.second));

		if (maximum_.first) {
			if (exclusiveMaximum_ && value >= maximum_.second)
				oss << "instance exceeds or equals maximum of " << json_schema::dump(json(maximum_.second));
			else if (value > maximum_.second)
				oss << "instance exceeds maximum of " << json_schema::dump(json(maximum_.second));
		}

		if (minimum_.first) {
			if (exclusiveMinimum_ && value <= minimum_.second)
				oss << "instance is below or equals minimum of " << json_schema::dump(json(minimum_.second));
			else if (value < minimum_.second)
				oss << "instance is below minimum of " << json_schema::dump(json(minimum_.second));
		}

		oss.seekp(0, std::ios::end);
		auto size = oss.tellp();
		if (size != 0) {
			oss.seekp(0, std::ios::beg);
			e.error(ptr, instance, oss.str());
		}
	}

public:
	numeric(const json &sch, root_schema *root, std::set<std::string> &kw)
	    : schema(root)
	{
		const auto &object = sch.get_object();
		auto attr = object.find("maximum");
		if (attr != object.end()) {
			maximum_ = {true, attr->second.as<T>()};
			kw.insert("maximum");
		}

		attr = object.find("minimum");
		if (attr != object.end()) {
			minimum_ = {true, attr->second.as<T>()};
			kw.insert("minimum");
		}

		attr = object.find("exclusiveMaximum");
		if (attr != object.end()) {
			exclusiveMaximum_ = true;
			maximum_ = {true, attr->second.as<T>()};
			kw.insert("exclusiveMaximum");
		}

		attr = object.find("exclusiveMinimum");
		if (attr != object.end()) {
			exclusiveMinimum_ = true;
			minimum_ = {true, attr->second.as<T>()};
			kw.insert("exclusiveMinimum");
		}

		attr = object.find("multipleOf");
		if (attr != object.end()) {
			multipleOf_ = {true, attr->second.as<double>()};
			kw.insert("multipleOf");
		}
	}
};

class null : public schema
{
	void validate(const json_pointer &ptr, const json &instance, json_patch &, error_handler &e) const override
	{
		if (!instance.is_null())
			e.error(ptr, instance, "expected to be null");
	}

public:
	null(json &, root_schema *root)
	    : schema(root) {}
};

class boolean_type : public schema
{
	void validate(const json_pointer &, const json &, json_patch &, error_handler &) const override {}

public:
	boolean_type(json &, root_schema *root)
	    : schema(root) {}
};

class boolean : public schema
{
	bool true_;
	void validate(const json_pointer &ptr, const json &instance, json_patch &, error_handler &e) const override
	{
		if (!true_) { // false schema
			// empty array
			// switch (instance.type()) {
			// case json::value_t::array:
			//	if (instance.size() != 0) // valid false-schema
			//		e.error(ptr, instance, "false-schema required empty array");
			//	return;
			//}

			e.error(ptr, instance, "instance invalid as per false-schema");
		}
	}

public:
	boolean(json &sch, root_schema *root)
	    : schema(root), true_(sch.get<bool>()) {}
};

class required : public schema
{
	const std::vector<std::string> required_;

	void validate(const json_pointer &ptr, const json &instance, json_patch &, error_handler &e) const override final
	{
		for (auto &r : required_)
			if (!instance.contains(r))
				e.error(ptr, instance, "required property '" + r + "' not found in object as a dependency");
	}

public:
	required(const std::vector<std::string> &r, root_schema *root)
	    : schema(root), required_(r) {}
};

class object : public schema
{
	std::pair<bool, size_t> maxProperties_{false, 0};
	std::pair<bool, size_t> minProperties_{false, 0};
	std::vector<std::string> required_;

	std::map<std::string, std::shared_ptr<schema>> properties_;
#ifndef NO_STD_REGEX
	std::vector<std::pair<REGEX_NAMESPACE::regex, std::shared_ptr<schema>>> patternProperties_;
#endif
	std::shared_ptr<schema> additionalProperties_;

	std::map<std::string, std::shared_ptr<schema>> dependencies_;

	std::shared_ptr<schema> propertyNames_;

	void validate(const json_pointer &ptr, const json &instance, json_patch &patch, error_handler &e) const override
	{
		if (maxProperties_.first && instance.size() > maxProperties_.second)
			e.error(ptr, instance, "too many properties");

		if (minProperties_.first && instance.size() < minProperties_.second)
			e.error(ptr, instance, "too few properties");

		for (auto &r : required_)
			if (!instance.contains(r))
				e.error(ptr, instance, "required property '" + r + "' not found in object");

		// for each property in instance
		for_each_item(instance, [&](const std::string &key, const json &value) {
			if (propertyNames_)
				propertyNames_->validate(ptr, json(key), patch, e);

			bool a_prop_or_pattern_matched = false;
			auto schema_p = properties_.find(key);
			// check if it is in "properties"
			if (schema_p != properties_.end()) {
				a_prop_or_pattern_matched = true;
				schema_p->second->validate(ptr / key, value, patch, e);
			}

#ifndef NO_STD_REGEX
			// check all matching patternProperties
			for (auto &schema_pp : patternProperties_)
				if (REGEX_NAMESPACE::regex_search(key, schema_pp.first)) {
					a_prop_or_pattern_matched = true;
					schema_pp.second->validate(ptr / key, value, patch, e);
				}
#endif

			// check additionalProperties as a last resort
			if (!a_prop_or_pattern_matched && additionalProperties_) {
				first_error_handler additional_prop_err;
				additionalProperties_->validate(ptr / key, value, patch, additional_prop_err);
				if (additional_prop_err)
					e.error(ptr, instance, "validation failed for additional property '" + key + "': " + additional_prop_err.message_);
			}
		});

		// reverse search
		for (auto const &prop : properties_) {
			if (!instance.contains(prop.first)) { // if the prop is not in the instance
				const auto &default_value = prop.second->default_value(ptr, instance, e);
				if (!default_value.is_null()) { // if default value is available
					patch.add((ptr / prop.first), default_value);
				}
			}
		}

		for (auto &dep : dependencies_) {
			if (instance.contains(dep.first))                              // if dependency-property is present in instance
				dep.second->validate(ptr / dep.first, instance, patch, e); // validate
		}
	}

public:
	object(json &sch,
	       root_schema *root,
	       const std::vector<json_uri> &uris)
	    : schema(root)
	{
		auto &object = sch.get_object();
		auto attr = object.find("maxProperties");
		if (attr != object.end()) {
			maxProperties_ = {true, attr->second.as<size_t>()};
			object.erase(attr);
		}

		attr = object.find("minProperties");
		if (attr != object.end()) {
			minProperties_ = {true, attr->second.as<size_t>()};
			object.erase(attr);
		}

		attr = object.find("required");
		if (attr != object.end()) {
			required_ = string_list(attr->second);
			object.erase(attr);
		}

		attr = object.find("properties");
		if (attr != object.end()) {
			for_each_item(attr->second, [&](const std::string &key, json &value) {
				properties_.insert(
				    std::make_pair(
				        key,
				        schema::make(value, root, {"properties", key}, uris)));
			});
			object.erase(attr);
		}

#ifndef NO_STD_REGEX
		attr = object.find("patternProperties");
		if (attr != object.end()) {
			for_each_item(attr->second, [&](const std::string &key, json &value) {
				patternProperties_.push_back(
				    std::make_pair(
				        REGEX_NAMESPACE::regex(key, REGEX_NAMESPACE::regex::ECMAScript),
				        schema::make(value, root, {key}, uris)));
			});
			object.erase(attr);
		}
#endif

		attr = object.find("additionalProperties");
		if (attr != object.end()) {
			additionalProperties_ = schema::make(attr->second, root, {"additionalProperties"}, uris);
			object.erase(attr);
		}

		attr = object.find("dependencies");
		if (attr != object.end()) {
			for_each_item(attr->second, [&](const std::string &key, json &value) {
				switch (type_of(value)) {
				case value_t::array:
					dependencies_.emplace(key,
					                      std::make_shared<required>(
					                          string_list(value), root));
					break;

				default:
					dependencies_.emplace(key,
					                      schema::make(value, root, {"dependencies", key}, uris));
					break;
				}
			});
			object.erase(attr);
		}

		attr = object.find("propertyNames");
		if (attr != object.end()) {
			propertyNames_ = schema::make(attr->second, root, {"propertyNames"}, uris);
			object.erase(attr);
		}

		attr = object.find("default");
		if (attr != object.end()) {
			set_default_value(attr->second);
		}
	}
};

class array : public schema
{
	std::pair<bool, size_t> maxItems_{false, 0};
	std::pair<bool, size_t> minItems_{false, 0};
	bool uniqueItems_ = false;

	std::shared_ptr<schema> items_schema_;

	std::vector<std::shared_ptr<schema>> items_;
	std::shared_ptr<schema> additionalItems_;

	std::shared_ptr<schema> contains_;

	void validate(const json_pointer &ptr, const json &instance, json_patch &patch, error_handler &e) const override
	{
		if (maxItems_.first && instance.size() > maxItems_.second)
			e.error(ptr, instance, "array has too many items");

		if (minItems_.first && instance.size() < minItems_.second)
			e.error(ptr, instance, "array has too few items");

		const auto &elements = instance.get_array();

		if (uniqueItems_) {
			for (auto it = elements.cbegin(); it != elements.cend(); ++it) {
				auto v = std::find_if(it + 1, elements.cend(), [&](const json &other) { return json_schema::equal(other, *it); });
				if (v != elements.cend())
					e.error(ptr, instance, "items have to be unique for this array");
			}
		}

		size_t index = 0;
		if (items_schema_)
			for (auto &i : elements) {
				items_schema_->validate(ptr / index, i, patch, e);
				index++;
			}
		else {
			auto item = items_.cbegin();
			for (auto &i : elements) {
				std::shared_ptr<schema> item_validator;
				if (item == items_.cend())
					item_validator = additionalItems_;
				else {
					item_validator = *item;
					item++;
				}

				if (!item_validator)
					break;

				item_validator->validate(ptr / index, i, patch, e);
			}
		}

		if (contains_) {
			bool contained = false;
			for (auto &item : elements) {
				first_error_handler local_e;
				contains_->validate(ptr, item, patch, local_e);
				if (!local_e) {
					contained = true;
					break;
				}
			}
			if (!contained)
				e.error(ptr, instance, "array does not contain required element as per 'contains'");
		}
	}

public:
	array(json &sch, root_schema *root, const std::vector<json_uri> &uris)
	    : schema(root)
	{
		auto &object = sch.get_object();
		auto attr = object.find("maxItems");
		if (attr != object.end()) {
			maxItems_ = {true, attr->second.as<size_t>()};
			object.erase(attr);
		}

		attr = object.find("minItems");
		if (attr != object.end()) {
			minItems_ = {true, attr->second.as<size_t>()};
			object.erase(attr);
		}

		attr = object.find("uniqueItems");
		if (attr != object.end()) {
			uniqueItems_ = attr->second.get<bool>();
			object.erase(attr);
		}

		attr = object.find("items");
		if (attr != object.end()) {

			if (type_of(attr->second) == value_t::array) {
				size_t c = 0;
				for (auto &subsch : attr->second.get_array())
					items_.push_back(schema::make(subsch, root, {"items", std::to_string(c++)}, uris));

				auto attr_add = object.find("additionalItems");
				if (attr_add != object.end()) {
					additionalItems_ = schema::make(attr_add->second, root, {"additionalItems"}, uris);
					object.erase(attr_add);
				}

			} else if (type_of(attr->second) == value_t::object ||
			           type_of(attr->second) == value_t::boolean)
				items_schema_ = schema::make(attr->second, root, {"items"}, uris);

			object.erase(attr);
		}

		attr = object.find("contains");
		if (attr != object.end()) {
			contains_ = schema::make(attr->second, root, {"contains"}, uris);
			object.erase(attr);
		}
	}
};

std::shared_ptr<schema> type_schema::make(json &schema,
                                          value_t type,
                                          root_schema *root,
                                          const std::vector<json_uri> &uris,
                                          std::set<std::string> &kw)
{
	switch (type) {
	case value_t::null:
		return std::make_shared<null>(schema, root);

	case value_t::number_unsigned:
	case value_t::number_integer:
		return std::make_shared<numeric<std::int64_t>>(schema, root, kw);
	case value_t::number_float:
		return std::make_shared<numeric<double>>(schema, root, kw);
	case value_t::string:
		return std::make_shared<string>(schema, root);
	case value_t::boolean:
		return std::make_shared<boolean_type>(schema, root);
	case value_t::object:
		return std::make_shared<object>(schema, root, uris);
	case value_t::array:
		return std::make_shared<array>(schema, root, uris);

	case value_t::discarded: // not a real type - silence please
		break;

	case value_t::binary:
		break;
	}
	return nullptr;
}
} // namespace

namespace
{

std::shared_ptr<schema> schema::make(json &schema,
                                     root_schema *root,
                                     const std::vector<std::string> &keys,
                                     std::vector<json_uri> uris)
{
	// remove URIs which contain plain name identifiers, as sub-schemas cannot be referenced
	for (auto uri = uris.begin(); uri != uris.end();)
		if (uri->identifier() != "")
			uri = uris.erase(uri);
		else
			uri++;

	// append to all URIs the keys for this sub-schema
	for (auto &key : keys)
		for (auto &uri : uris)
			uri = uri.append(key);

	std::shared_ptr<::schema> sch;

	// boolean schema
	if (type_of(schema) == value_t::boolean)
		sch = std::make_shared<boolean>(schema, root);
	else if (type_of(schema) == value_t::object) {

		auto &object = schema.get_object();
		auto attr = object.find("$id"); // if $id is present, this schema can be referenced by this ID
		                                // as an additional URI
		if (attr != object.end()) {
			if (std::find(uris.begin(),
			              uris.end(),
			              attr->second.get<std::string>()) == uris.end())
				uris.push_back(uris.back().derive(attr->second.get<std::string>())); // so add it to the list if it is not there already
			object.erase(attr);
		}

		auto findDefinitions = [&](const std::string &defs) -> bool {
			attr = object.find(defs);
			if (attr != object.end()) {
				for_each_item(attr->second, [&](const std::string &key, json &value) {
					schema::make(value, root, {defs, key}, uris);
				});
				object.erase(attr);
				return true;
			}
			return false;
		};
		if (!findDefinitions("$defs")) {
			findDefinitions("definitions");
		}

		attr = object.find("$ref");
		if (attr != object.end()) { // this schema is a reference
			// the last one on the uri-stack is the last id seen before coming here,
			// so this is the origial URI for this reference, the $ref-value has thus be resolved from it
			auto id = uris.back().derive(attr->second.get<std::string>());
			sch = root->get_or_create_ref(id);

			object.erase(attr);

			// special case where we break draft-7 and allow overriding of properties when a $ref is used
			attr = object.find("default");
			if (attr != object.end()) {
				// copy the referenced schema depending on the underlying type and modify the default value
				if (auto new_sch = sch->make_for_default_(sch, root, uris, attr->second)) {
					sch = new_sch;
				}
				object.erase(attr);
			}
		} else {
			sch = std::make_shared<type_schema>(schema, root, uris);
		}

		erase_key(schema, "$schema");
		erase_key(schema, "title");
		erase_key(schema, "description");
	} else {
		throw std::invalid_argument("invalid JSON-type for a schema for " + uris[0].to_string() + ", expected: boolean or object");
	}

	for (auto &uri : uris) { // for all URIs this schema is referenced by
		root->insert(uri, sch);

		if (type_of(schema) == value_t::object)
			for_each_item(schema, [&](const std::string &key, json &value) {
				root->insert_unknown_keyword(uri, key, value); // insert unknown keywords for later reference
			});
	}
	return sch;
}

class throwing_error_handler : public error_handler
{
	void error(const json_pointer &ptr, const json &instance, const std::string &message) override
	{
		throw std::invalid_argument(std::string("At ") + ptr.to_string() + " of " + json_schema::dump(instance) + " - " + message + "\n");
	}
};

} // namespace

namespace json_schema
{

json_validator::json_validator(schema_loader loader,
                               format_checker format,
                               content_checker content)
    : root_(std::unique_ptr<root_schema>(new root_schema(std::move(loader),
                                                         std::move(format),
                                                         std::move(content))))
{
}

json_validator::json_validator(const json &schema,
                               schema_loader loader,
                               format_checker format,
                               content_checker content)
    : json_validator(std::move(loader),
                     std::move(format),
                     std::move(content))
{
	set_root_schema(schema);
}

json_validator::json_validator(json &&schema,
                               schema_loader loader,
                               format_checker format,
                               content_checker content)

    : json_validator(std::move(loader),
                     std::move(format),
                     std::move(content))
{
	set_root_schema(std::move(schema));
}

// move constructor, destructor and move assignment operator can be defaulted here
// where root_schema is a complete type
json_validator::json_validator(json_validator &&) = default;
json_validator::~json_validator() = default;
json_validator &json_validator::operator=(json_validator &&) = default;

void json_validator::set_root_schema(const json &schema)
{
	root_->set_root_schema(schema);
}

void json_validator::set_root_schema(json &&schema)
{
	root_->set_root_schema(std::move(schema));
}

json json_validator::validate(const json &instance) const
{
	throwing_error_handler err;
	return validate(instance, err);
}

json json_validator::validate(const json &instance, error_handler &err, const json_uri &initial_uri) const
{
	json_pointer ptr;
	json_patch patch;
	root_->validate(ptr, instance, patch, err, initial_uri);
	return patch;
}

} // namespace json_schema
