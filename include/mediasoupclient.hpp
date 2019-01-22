#ifndef MEDIASOUP_CLIENT_HPP
#define MEDIASOUP_CLIENT_HPP

#include "Consumer.hpp"
#include "Device.hpp"
#include "Producer.hpp"
#include "Transport.hpp"

using SendTransportListener = SendTransport::Listener;
using TransportListener     = Transport::Listener;
using ConsumerListener      = Consumer::PublicListener;
using ProducerListener      = Producer::PublicListener;

#endif
