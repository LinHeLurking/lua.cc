function check_student(student)
    print("[Lua] Called `check_student`. Setting fields in lua...")
    student.name = "Cloud"
    student.age = 13
    print("[Lua] " .. student.name)
    print("[Lua] " .. student.age)
    student:greet()
end
