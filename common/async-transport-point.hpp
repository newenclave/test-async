#ifndef ASYNC_TRANSPORT_POINT_HPP
#define ASYNC_TRANSPORT_POINT_HPP

#include "boost/asio.hpp"
#include "boost/enable_shared_from_this.hpp"
#include "boost/bind.hpp"
#include "boost/make_shared.hpp"
#include "boost/weak_ptr.hpp"

#include <string>
#include <queue>

#include "vtrc-signal-declaration.h"

namespace async_transport {

    template <typename ST>
    class point_iface: public boost::enable_shared_from_this<point_iface<ST> > {

        typedef point_iface<ST> this_type;

    public:

        typedef boost::shared_ptr<this_type> shared_type;
        typedef boost::weak_ptr<this_type>   weak_type;
        typedef ST stream_type;

    private:

        typedef std::string messate_type;

        struct queue_container {

            typedef boost::shared_ptr<queue_container> shared_type;

            messate_type message_;

            queue_container( const char *data, size_t length )
                :message_(data, length)
            { }

            static shared_type create( const char *data, size_t length )
            {
                return boost::make_shared<queue_container>( data, length );
            }
        };

        typedef typename queue_container::shared_type  queue_container_sptr;

        typedef std::queue<queue_container_sptr> message_queue_type;

        typedef void (this_type::*read_impl)( );
        typedef int  priority_type;

        boost::asio::io_service          &ios_;
        boost::asio::io_service::strand   write_dispatcher_;
        stream_type                       stream_;

        message_queue_type                write_queue_;

        std::vector<char>                 read_buffer_;
        read_impl                         read_impl_;

        bool                              active_;

        static
        read_impl get_dispatch( bool dispatch )
        {
            return dispatch
                    ? &this_type::start_read_impl_wrap
                    : &this_type::start_read_impl;
        }

    protected:

        point_iface( boost::asio::io_service &ios,
                     size_t read_block_size = 4096,
                     bool dispatch_read = false)
            :ios_(ios)
            ,write_dispatcher_(ios_)
            ,stream_(ios_)
            ,read_buffer_(read_block_size)
            ,read_impl_(get_dispatch(dispatch_read))
            ,active_(true)
        { }

    private:

        void close_impl(  )
        {
            if( active_ ) {
                active_ = false;
                stream_.close( );
            }
        }

        void push_close(  )
        {
            write_dispatcher_.post(
                        boost::bind( &this_type::close_impl,
                                     this->shared_from_this( ) ));
        }

        void queue_push( const queue_container_sptr &new_mess )
        {
            write_queue_.push( new_mess );
        }

        const queue_container_sptr &queue_top( )
        {
            return write_queue_.front( );
        }

        void queue_pop( )
        {
            write_queue_.pop( );
        }

        bool queue_empty( ) const
        {
            return write_queue_.empty( );
        }

        /// ================ write ================ ///
        void async_write( const char *data, size_t length, size_t total )
        {
            try {
                stream_.async_write_some(
                    boost::asio::buffer( data, length ),
                        write_dispatcher_.wrap(
                            boost::bind( &this_type::write_handler, this,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred,
                            length, total,
                            this->shared_from_this( ))
                        )
                    );
            } catch( const std::exception & ) {
                ;;; /// generate error
            }
        }

        void async_write(  )
        {
            std::string &top( queue_top( )->message_ );

            top.assign( on_transform_message( top ) );

            async_write( top.c_str( ), top.size( ), 0);
        }

        void write_handler( const boost::system::error_code &error,
                            size_t const bytes,
                            size_t const length,
                            size_t       total,
                            shared_type  /*this_inst*/)
        {
            queue_container &top( *queue_top( ) );

            if( !error ) {

                if( bytes < length ) {

                    total += bytes;

                    const std::string &top_mess( top.message_ );
                    async_write(top_mess.c_str( ) + total,
                                top_mess.size( )  - total, total);

                } else {

                    queue_pop( );

                    if( !queue_empty( ) ) {
                        async_write(  );
                    }

                }
            } else {
                /// generate error
                on_write_error( error );
            }

        }

        void write_impl( const queue_container_sptr data, shared_type /*inst*/ )
        {
            const bool empty = queue_empty( );

            queue_push( data );

            if( empty ) {
                async_write(  );
            }
        }

        void push_write( const char *data, size_t len )
        {
            queue_container_sptr inst(queue_container::create( data, len ));

            write_dispatcher_.post(
                    boost::bind( &this_type::write_impl, this,
                                 inst, this->shared_from_this( ) )
                    );
        }

        /// ================ read ================ ///
        void read_handler( const boost::system::error_code &error,
                           size_t const bytes, shared_type /*inst*/ )
        {
            if( !error ) {
                on_read( &read_buffer_[0], bytes );
                push_start_read( );
            } else {
                /// genegate error;
                on_read_error( error );
            }
        }

        void start_read_impl_wrap(  )
        {
            stream_.async_read_some(
                    boost::asio::buffer(&read_buffer_[0], read_buffer_.size( )),
                    write_dispatcher_.wrap(
                        boost::bind( &this_type::read_handler, this,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred,
                        this->shared_from_this( ))
                    )
             );
        }

        void start_read_impl(  )
        {
            stream_.async_read_some(
                boost::asio::buffer(&read_buffer_[0], read_buffer_.size( )),
                        boost::bind( &this_type::read_handler, this,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred,
                            this->shared_from_this( )
                        )
                );
        }

        void push_start_read( )
        {
            (this->*read_impl_)( );
        }

    private:

        virtual void on_read( const char *data, size_t length ) = 0;

        virtual void on_read_error( const boost::system::error_code &code )
        { }

        virtual void on_write_error( const boost::system::error_code &code )
        { }

        virtual std::string on_transform_message( std::string &message )
        {
            return message;
        }

    public:

        virtual ~point_iface( ) { }

        boost::asio::io_service &get_io_service( )
        {
            return ios_;
        }

        const boost::asio::io_service &get_io_service( ) const
        {
            return ios_;
        }

        const boost::asio::io_service::strand &get_dispatcher( ) const
        {
            return write_dispatcher_;
        }

        boost::asio::io_service::strand &get_dispatcher( )
        {
            return write_dispatcher_;
        }

        stream_type &get_stream( )
        {
            return stream_;
        }

        const stream_type &get_stream( ) const
        {
            return stream_;
        }

        void write( const std::string &data )
        {
            write( data.c_str( ), data.size( ) );
        }

        void write( const char *data, size_t length )
        {
            push_write( data, length );
        }

        void start_read( )
        {
            push_start_read( );
        }

        void close( )
        {
            push_close( );
        }

    };

}


#endif // ASYNC_TRANSPORT_POINT_HPP
