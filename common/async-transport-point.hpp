#ifndef ASYNC_TRANSPORT_POINT_HPP
#define ASYNC_TRANSPORT_POINT_HPP

#include "boost/asio.hpp"
#include "boost/enable_shared_from_this.hpp"
#include "boost/bind.hpp"
#include "boost/make_shared.hpp"
#include "boost/weak_ptr.hpp"

#include <deque>
#include <string>

#include "vtrc-signal-declaration.h"

namespace async_transport {

    struct write_transformer {
        virtual std::string transform( std::string &data ) = 0;
    };

    struct write_transformer_none: public write_transformer {
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
            char         priority_;
            messate_type message_;
            queue_container( )
                :priority_(0)
            { }
        };

        typedef boost::shared_ptr<queue_container> queue_container_sptr;

        struct impl: public boost::enable_shared_from_this<impl> {

            typedef ST stream_type;
            typedef boost::shared_ptr<impl> shared_type;
            typedef void ( impl::*read_impl )( );

            boost::asio::io_service          &ios_;
            boost::asio::io_service::strand   write_dispatcher_;
            stream_type                       stream_;

            std::deque<queue_container_sptr>  write_queue_;
            std::vector<char>                 read_buffer_;
            read_impl                         read_impl_;

            point_iface<stream_type>         *parent_;

            bool                              active_;

            boost::scoped_ptr<write_transformer> transformer_;

            impl( boost::asio::io_service &ios, size_t read_block_size )
                :ios_(ios)
                ,write_dispatcher_(ios_)
                ,stream_(ios_)
                ,read_buffer_(read_block_size)
                ,read_impl_(&impl::start_read_impl_wrap)
                ,active_(true)
                ,transformer_(new write_transformer_none)
            { }

            void set_transformer_impl( write_transformer *new_trans )
            {
                transformer_.reset(new_trans);
            }

            void set_transformer( write_transformer *new_trans )
            {
                write_dispatcher_.post(
                            boost::bind( &impl::set_transformer_impl,
                                         this->shared_from_this( ),
                                         new_trans ));
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
                                this->shared_from_this( )))
                            );
                } catch( const std::exception & ) {
                    ;;; /// generate error
                }
            }

            void async_write(  )
            {
                std::string &top(write_queue_.front( )->message_);
                top.assign(transformer_->transform( top ) );

                async_write( write_queue_.front( )->message_.c_str( ),
                             write_queue_.front( )->message_.size( ), 0);
            }

            void write_handler( const boost::system::error_code &error,
                                size_t const bytes,
                                size_t const length,
                                size_t       total,
                                shared_type  /*this_inst*/)
            {
                queue_container &top( *write_queue_.front( ) );

                if( !error ) {

                    if( bytes < length ) {

                        total += bytes;

                        const std::string &top_mess(top.message_);
                        async_write(top_mess.c_str( ) + total,
                                    top_mess.size( )  - total, total);

                    } else {

                        write_queue_.pop_front( );

                        if( !write_queue_.empty( ) )
                            async_write(  );
                    }
                } else {
                    parent_->on_write_error_( error );
                    /// generate error
                }

            }

            void write_impl( char /*priority*/, const queue_container_sptr data,
                             shared_type /*inst*/ )
            {
                bool queue_empty = write_queue_.empty( );
                write_queue_.push_back( data );

                if( queue_empty ) {
                    async_write(  );
                }
            }

            /// ================ read ================ ///
            void read_handler( const boost::system::error_code &error,
                               size_t const bytes, shared_type /*weak_inst*/ )
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
                            this->shared_from_this( )))
                    );
            }

            void start_read_impl(  )
            {
                stream_.async_read_some(
                    boost::asio::buffer(&read_buffer_[0], read_buffer_.size( )),
                            boost::bind( &impl::read_handler, this,
                                boost::asio::placeholders::error,
                                boost::asio::placeholders::bytes_transferred,
                                this->shared_from_this( ))
                    );
            }

            void start_read( )
            {
                (this->*read_impl_)( );
            }

            void write( const char *data, size_t length )
            {
                queue_container_sptr inst(
                                    boost::make_shared<queue_container>( ));
                inst->message_.assign( data, length );
                write_dispatcher_.post(
                        boost::bind( &impl::write_impl, this,
                                     0, inst, this->shared_from_this( ) ) );
            }

        };

    public:

        typedef ST stream_type;
        typedef boost::shared_ptr<this_type> shared_type;
        typedef boost::weak_ptr<this_type>   weak_type;

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

        void set_transformer( write_transformer *new_trans )
        {
            impl_->set_transformer( new_trans );
        }

    };

}


#endif // ASYNC_TRANSPORT_POINT_HPP
