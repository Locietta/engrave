// Ported to glaze for engrave: namespace json_schema, json-dom.hpp's model.
#include "json-patch.hpp"

#include "json-schema.hpp"

namespace
{

// originally from http://jsonpatch.com/, http://json.schemastore.org/json-patch
// with fixes
const json_schema::json patch_schema = json_schema::parse(R"patch({
    "title": "JSON schema for JSONPatch files",
    "$schema": "http://json-schema.org/draft-04/schema#",
    "type": "array",

    "items": {
        "oneOf": [
            {
                "additionalProperties": false,
                "required": [ "value", "op", "path"],
                "properties": {
                    "path" : { "$ref": "#/definitions/path" },
                    "op": {
                        "description": "The operation to perform.",
                        "type": "string",
                        "enum": [ "add", "replace", "test" ]
                    },
                    "value": {
                        "description": "The value to add, replace or test."
                    }
                }
            },
            {
                "additionalProperties": false,
                "required": [ "op", "path"],
                "properties": {
                    "path" : { "$ref": "#/definitions/path" },
                    "op": {
                        "description": "The operation to perform.",
                        "type": "string",
                        "enum": [ "remove" ]
                    }
                }
            },
            {
                "additionalProperties": false,
                "required": [ "from", "op", "path" ],
                "properties": {
                    "path" : { "$ref": "#/definitions/path" },
                    "op": {
                        "description": "The operation to perform.",
                        "type": "string",
                        "enum": [ "move", "copy" ]
                    },
                    "from": {
                        "$ref": "#/definitions/path",
                        "description": "A JSON Pointer path pointing to the location to move/copy from."
                    }
                }
            }
        ]
    },
    "definitions": {
        "path": {
            "description": "A JSON Pointer path.",
            "type": "string"
        }
    }
})patch");
} // namespace

namespace json_schema
{

namespace
{
json make_operation(const char *op, const json_pointer &ptr)
{
	json operation = json::object_t{};
	operation["op"] = op;
	operation["path"] = ptr.to_string();
	return operation;
}
} // namespace

json_patch::json_patch(json &&patch)
    : j_(std::move(patch))
{
	validateJsonPatch(j_);
}

json_patch::json_patch(const json &patch)
    : j_(std::move(patch))
{
	validateJsonPatch(j_);
}

json_patch &json_patch::add(const json_pointer &ptr, json value)
{
	json operation = make_operation("add", ptr);
	operation["value"] = std::move(value);
	j_.get_array().push_back(std::move(operation));
	return *this;
}

json_patch &json_patch::replace(const json_pointer &ptr, json value)
{
	json operation = make_operation("replace", ptr);
	operation["value"] = std::move(value);
	j_.get_array().push_back(std::move(operation));
	return *this;
}

json_patch &json_patch::remove(const json_pointer &ptr)
{
	j_.get_array().push_back(make_operation("remove", ptr));
	return *this;
}

void json_patch::validateJsonPatch(json const &patch)
{
	// static put here to have it created at the first usage of validateJsonPatch
	static json_validator patch_validator(patch_schema);

	patch_validator.validate(patch);

	for (auto const &op : patch.get_array())
		json_pointer(op["path"].get<std::string>());
}

} // namespace json_schema
