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

    struct message_transformer {
        virtual std::string transform( std::string &data ) = 0;
    };

    struct write_transformer_none: public message_transformer {
        std::string transform( std::string &data )
        {
            return data;
        }
    };

    template <typename ST>
    class point_iface {

        typedef point_iface<ST> this_type;

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
        typedef boost::shared_ptr<message_transformer> transformer_sptr;

        typedef std::queue<queue_container_sptr> message_queue_type;

        struct impl: public boost::enable_shared_from_this<impl> {

            typedef ST stream_type;
            typedef boost::shared_ptr<impl> shared_type;
            typedef void (impl::*read_impl)( );
            typedef int  priority_type;

            boost::asio::io_service          &ios_;
            boost::asio::io_service::strand   write_dispatcher_;
            stream_type                       stream_;

            message_queue_type                write_queue_;

            std::vector<char>                 read_buffer_;
            read_impl                         read_impl_;

            point_iface<stream_type>         *parent_;

            bool                              active_;

            transformer_sptr                  transformer_;

            impl( boost::asio::io_service &ios, size_t read_block_size )
                :ios_(ios)
                ,write_dispatcher_(ios_)
                ,stream_(ios_)
                ,read_buffer_(read_block_size)
                ,read_impl_(&impl::start_read_impl)
                ,active_(true)
                ,transformer_(new write_transformer_none)
            { }

            void set_transformer_impl( transformer_sptr transform )
            {
                transformer_ = transform;
            }

            void set_transformer( transformer_sptr transform )
            {
                write_dispatcher_.post(
                            boost::bind( &impl::set_transformer_impl,
                                         this->shared_from_this( ),
                                         transform ));
            }

            void close_impl(  )
            {
                if( active_ ) {
                    active_ = false;
                    stream_.close( );
                }
            }

            void close(  )
            {
                write_dispatcher_.post(
                            boost::bind( &impl::close_impl,
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
                                boost::bind( &impl::write_handler, this,
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

                top.assign( transformer_->transform( top ) );

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
                    parent_->on_write_error_( error );
                }

            }

            void write_impl( const queue_container_sptr data,
                             shared_type /*inst*/ )
            {
                bool empty = queue_empty( );

                queue_push( data );

                if( empty ) {
                    async_write(  );
                }
            }

            void write( const char *data, size_t len )
            {
                queue_container_sptr inst(queue_container::create( data, len ));

                write_dispatcher_.post(
                        boost::bind( &impl::write_impl, this,
                                     inst, this->shared_from_this( ) )
                        );
            }

            /// ================ read ================ ///
            void read_handler( const boost::system::error_code &error,
                               size_t const bytes, shared_type /*inst*/ )
            {
                if( !error ) {
                    parent_->on_read_( &read_buffer_[0], bytes );
                    start_read( );
                } else {
                    /// genegate error;
                    parent_->on_read_error_( error );
                }
            }

            void start_read_impl_wrap(  )
            {
                stream_.async_read_some(
                    boost::asio::buffer(&read_buffer_[0], read_buffer_.size( )),
                        write_dispatcher_.wrap(
                            boost::bind( &impl::read_handler, this,
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
                            boost::bind( &impl::read_handler, this,
                                boost::asio::placeholders::error,
                                boost::asio::placeholders::bytes_transferred,
                                this->shared_from_this( )
                            )
                    );
            }

            void start_read( )
            {
                (this->*read_impl_)( );
            }
        };

    public:

        typedef ST stream_type;
        typedef boost::shared_ptr<this_type> shared_type;
        typedef boost::weak_ptr<this_type>   weak_type;
        typedef typename impl::priority_type priority_type;

        struct observer {
            virtual ~observer( ) { }
        };

        struct error_observer: public observer {
            virtual void notify( const boost::system::error_code &error ) = 0;
        };

        struct read_observer: public observer {
            virtual void notify( const char *data, size_t length ) = 0;
        };

    private:

        boost::shared_ptr<impl> impl_;

        friend class impl;

        VTRC_DECLARE_SIGNAL( on_read, void ( const char *, size_t ) );

        VTRC_DECLARE_SIGNAL( on_read_error,
                             void ( const boost::system::error_code & ) );

        VTRC_DECLARE_SIGNAL( on_write_error,
                             void ( const boost::system::error_code & ) );

    public:

        point_iface( boost::asio::io_service &ios )
            :impl_(new impl(ios, 4096))
        {
            impl_->parent_ = this;
        }

        virtual ~point_iface( ) { }

    public:

        boost::asio::io_service &get_io_service( )
        {
            return impl_->ios_;
        }

        const boost::asio::io_service &get_io_service( ) const
        {
            return impl_->ios_;
        }

        stream_type &stream( )
        {
            return impl_->stream_;
        }

        const stream_type &stream( ) const
        {
            return impl_->stream_;
        }

        void write( const std::string &data )
        {
            write( data.c_str( ), data.size( ) );
        }

        void write( const char *data, size_t length )
        {
            impl_->write( data, length );
        }

        void start_read( )
        {
            impl_->start_read( );
        }

        void close( )
        {
            impl_->close( );
        }

        void set_transformer( message_transformer *new_trans )
        {
            impl_->set_transformer( transformer_sptr(new_trans) );
        }

    };

}


#endif // ASYNC_TRANSPORT_POINT_HPP
