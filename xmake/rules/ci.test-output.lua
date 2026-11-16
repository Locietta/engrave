local function _append_output(parts, kind, output)
    if not output or #output == 0 then
        return
    end

    table.insert(parts, kind .. ":\n")
    table.insert(parts, output)
    if output:sub(-1) ~= "\n" then
        table.insert(parts, "\n")
    end
end

rule("ci.test-output")
    after_test(function (_, test)
        if os.getenv("GITHUB_ACTIONS") ~= "true" or not test.errors then
            return
        end

        local output = {"::group::xmake test " .. test.name .. " diagnostics\n"}
        _append_output(output, "stdout", test.stdout)
        _append_output(output, "stderr", test.stderr)
        _append_output(output, "errors", test.errors)
        table.insert(output, "::endgroup::\n")
        io.write(table.concat(output))
    end)
