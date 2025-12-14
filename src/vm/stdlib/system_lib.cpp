namespace meow::natives::sys {

// system.argv() -> ["arg1", "arg2"]
static Value get_argv(Machine* vm, int argc, Value* argv) {
    const auto& cmd_args = vm->get_args().command_line_arguments_; 
        
    auto arr = vm->get_heap()->new_array();
    for (const auto& arg : cmd_args) {
        arr->push(Value(vm->get_heap()->new_string(arg)));
    }
    return Value(arr);
}

// system.exit(code)
static Value exit_vm(Machine* vm, int argc, Value* argv) {
    int code = 0;
    if (argc > 0) code = to_int(argv[0]);
    std::exit(code);
    std::unreachable();
    return Value(null_t{});
}

// system.exec(command)
static Value exec_cmd(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    std::string cmd = argv[0].as_string()->c_str();
    int code = std::system(cmd.c_str());
    return Value((int64_t)code);
}

// system.time() -> trả về timestamp (ms)
static Value time_now(Machine* vm, int argc, Value* argv) {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return Value((int64_t)ms);
}

} // namespace meow::natives::sys