function rewrite_data_dict(data_dict)
    print("\nRewriting data dict in lua!\n")
    -- Print metatable for debugging
    print("Metatable of arg: ")
    for k, v in pairs(getmetatable(data_dict)) do
        print(k , v)
    end
    print("\n")

    -- x:y(a, b) is equvilant with x.(x, a, b) (passing self)
    data_dict:print_info()

    data_dict:set_field("a", -1)
    data_dict:set_field("b", -2)
    data_dict:set_field("c", -3)
    data_dict:set_field("d", -4)

    data_dict:print_info()
end
