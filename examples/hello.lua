function string_response(str)
    local done = false
    return function()
        if not done then
            done = true
            return str
        end
    end
end

serve(1234, function(req)
    return 200, {["Content-Type"] = "text/plain"}, string_response("Hello world")
end)
