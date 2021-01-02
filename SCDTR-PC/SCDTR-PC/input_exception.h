#ifndef INPUT_EXCEPTION_H
#define INPUT_EXCEPTION_H

class input_exception : public std::exception
{
public:
    input_exception(std::string const &message) : msg_(message) {}
    virtual char const *what() const noexcept { return msg_.c_str(); }

private:
    std::string msg_;
};

#endif