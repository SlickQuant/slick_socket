#pragma once

namespace slick_socket
{

inline TCPServerBase::TCPServerBase(ITCPServerCallback* callback, const Config& config, ILogger* logger)
    : impl_(std::make_unique<Impl>(config, callback, logger))
{
}

inline TCPServerBase::~TCPServerBase() = default;
inline TCPServerBase::TCPServerBase(TCPServerBase&& other) noexcept = default;
inline TCPServerBase& TCPServerBase::operator=(TCPServerBase&& other) noexcept = default;

inline bool TCPServerBase::start()
{
    return impl_->start();
}

inline void TCPServerBase::stop()
{
    impl_->stop();
}

inline bool TCPServerBase::is_running() const
{
    return impl_->is_running();
}

inline bool TCPServerBase::send_data(int client_id, const std::vector<uint8_t>& data)
{
    return impl_->send_data(client_id, data);
}

inline bool TCPServerBase::send_data(int client_id, const std::string& data)
{
    return impl_->send_data(client_id, data);
}

inline void TCPServerBase::disconnect_client(int client_id)
{
    impl_->disconnect_client(client_id);
}

inline size_t TCPServerBase::get_connected_client_count() const
{
    return impl_->get_connected_client_count();
}

} // namespace slick_socket