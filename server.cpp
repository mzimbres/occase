#include <stack>
#include <memory>
#include <string>
#include <chrono>
#include <vector>
#include <thread>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <unordered_set>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <nlohmann/json.hpp>

#include "config.hpp"
#include "user.hpp"

using json = nlohmann::json;

// This is the type we will use to associate a user  with something
// that identifies them in the real world like their phone numbers or
// email.
using id_type = std::string;

// Items will be added in the vector and wont be removed, instead, we
// will push its index in the stack. When a new item is requested we
// will pop one index from the stack and use it, if none is available
// we push_back in the vector.
template <class T>
class grow_only_vector {
public:
   // I do not want an unsigned index type.
   //using size_type = typename std::vector<T>::size_type;
   using size_type = int;
   using reference = typename std::vector<T>::reference;
   using const_reference = typename std::vector<T>::const_reference;

private:
   std::stack<size_type> avail;
   std::vector<T> items;

public:
   reference operator[](size_type i)
   { return items[i]; };

   const_reference operator[](size_type i) const
   { return items[i]; };

   // Returns the index of an element int the group that is free
   // for use.
   auto allocate()
   {
      if (avail.empty()) {
         auto size = items.size();
         items.push_back({});
         return static_cast<size_type>(size);
      }

      auto i = avail.top();
      avail.pop();
      return i;
   }

   void deallocate(size_type idx)
   {
      avail.push(idx);
   }

   auto is_valid_index(size_type idx) const noexcept
   {
      return idx >= 0
          && idx < static_cast<size_type>(items.size());
   }
};

enum class group_membership
{
   // Member is allowed to read posts and has no access to poster
   // contact. That means in practice he cannot buy anything but only
   // see the traffic in the group and decide whether it is worth to
   // updgrade.
   WATCH

   // Allowed to see posts, see poster contact and post. He gets posts
   // with delay of some hours and his posts are subject to whether he
   // has provided the bank details.
,  DEFAULT
   
   // Like DEFAULT but get posts without any delay.
,  PREMIUM
};

struct group_mem_info {
   group_membership membership {group_membership::DEFAULT};
   std::chrono::seconds delay {0};
};

enum class group_visibility
{
   // Members do not need auhorization to enter the group.
   PUBLIC

   // Only the owner can add members.
,  PRIVATE
};

class group {
private:
   group_visibility visibility {group_visibility::PUBLIC};
   
   index_type owner {-1}; // The user that owns this group.

   // The number of members in a group is expected be on the
   // thousands, let us say 100k. The operations performed are
   //
   // 1. Insert: Quite often. To avoid inserting twice we have search
   //            before inserting.
   // 2. Remove: Once in a while, also requires searching.
   // 3. Search: I am not sure yet, but for security reasons we may
   //            have to always check if the user has the right to
   //            announce in the group in case the app sends us an
   //            incorrect old group id.
   // 
   // Given those requirements above, I think a hash table is more
   // appropriate.
   std::unordered_map< index_type
                     , group_mem_info> members;

public:
   auto get_owner() const noexcept {return owner;}
   void set_owner(index_type idx) noexcept {owner = idx;}

   auto is_owned_by(index_type uid) const noexcept
   {
      return uid > 0 && owner == uid;
   }

   void reset()
   {
      owner = -1;
      members = {};
   }

   void add_member(index_type uid)
   {
      members.insert({uid, {}});
   }

   void remove_member(index_type uid)
   {
      members.erase(uid);
   }
};

class server_data {
private:
   grow_only_vector<group> groups;
   grow_only_vector<user> users;

   // May grow up to millions of users.
   std::unordered_map<id_type, index_type> id_to_idx_map;

public:

   // This function is used to add a new user when he first installs
   // the app and it sends the first message to the server.  It
   // basically allocates user entries internally.
   //
   // id:       User telephone.
   // contacts: Telephones of his contacts. 
   // return:   Index in the users vector that we will use to refer to
   //           him without having to perform searches. This is what
   //           we will return to the user to be stored in his app.
   auto add_user(id_type id, std::vector<id_type> contacts)
   {
      auto new_user = id_to_idx_map.insert({id, {}});
      if (!new_user.second) {
         // The user already exists in the system. This case can be
         // triggered by some conditions
         // 1. The user lost his phone and the number was assigned to
         //    a new person.
         // 2. He reinstalled the app.
         //
         // REVIEW: I still do not know how to handle this sitiations
         // properly so I am not going to do anything for now. We
         // could for example reset his data.
      }

      // The user did not exist in the system. We have to allocate
      // space for him.
      auto new_user_idx = users.allocate();
      
      // Now we can add all his cantacts that are already registered
      // in the app.
      for (auto const& o : contacts) {
         auto existing_user = id_to_idx_map.find(o);
         if (existing_user == std::end(id_to_idx_map))
            continue;

         // A contact was found in our database, let us add him as a
         // the new user friend. We do not have to check if it is
         // already in the array as case would handled by the set
         // automaticaly.
         auto idx = existing_user->second;
         users[new_user_idx].add_friend(idx);

         // REVIEW: We also have to inform the existing user that one of
         // his contacts just registered. This will be made only upon
         // request, for now, we will only add the new user in the
         // existing user's friends and let him be notified if the new
         // user sends him a message.
         users[idx].add_friend(new_user_idx);
      }

      return new_user_idx;
   }

   // TODO: update_user_contacts.

   // Adds new group for the specified owner and returns its index.
   auto add_group(index_type owner)
   {
      // Before allocating a new group it is a good idea to check if
      // the owner passed is at least in a valid range.
      if (users.is_valid_index(owner)) {
         // This is a non-existing user. Perhaps the json command was
         // sent with the wrong information signaling a logic error in
         // the app.
         return static_cast<index_type>(-1);
      }

      // We can proceed and allocate the group.
      auto idx = groups.allocate();
      groups[idx].set_owner(owner);

      // Updates the user with his new group.
      users[owner].add_group(idx);

      return idx;
   }

   // Removes the group and updates the owner.
   auto remove_group(index_type idx)
   {
      if (!groups.is_valid_index(idx))
         return group {}; // Out of range? Logic error.

      // To remove a group we have to inform its members the group has
      // been removed, therefore we will make a copy before removing
      // and return it.
      const auto removed_group = std::move(groups[idx]);

      // remove this line after implementing the swap idiom on the
      // group class.
      groups[idx].reset();

      groups.deallocate(idx);

      // Now we have to remove this group from the owners list.
      const auto owner = removed_group.get_owner();
      users[owner].remove_group(idx);
      return removed_group; // Let RVO optimize this.
   }

   auto change_group_ownership( index_type from, index_type to
                              , index_type gid)
   {
      if (!groups.is_valid_index(gid))
         return false;

      if (!groups[gid].is_owned_by(from))
         return false; // Sorry, you are not allowed.

      if (!users.is_valid_index(from))
         return false;

      if (!users.is_valid_index(to))
         return false;

      // The new owner exists.
      groups[gid].set_owner(to);
      users[to].add_group(gid);
      users[from].remove_group(gid);
      return true;
   }

   auto add_group_member( index_type owner, index_type new_member
                        , index_type gid)
   {
      if (!groups.is_valid_index(gid))
         return false;

      if (!groups[gid].is_owned_by(owner))
         return false;

      if (!users.is_valid_index(owner))
         return false;

      if (!users.is_valid_index(new_member))
         return false;
         
      groups[gid].add_member(new_member);
   }
};

using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;

void fail(boost::system::error_code ec, char const* what)
{
   std::cerr << what << ": " << ec.message() << "\n";
}

// Echoes back all received WebSocket messages
class session : public std::enable_shared_from_this<session>
{
private:
   websocket::stream<tcp::socket> ws;
   boost::asio::strand<
      boost::asio::io_context::executor_type> strand;
   boost::beast::multi_buffer buffer;
   std::shared_ptr<server_data> sd;

public:
   explicit session( tcp::socket socket
                   , std::shared_ptr<server_data> sd_)
   : ws(std::move(socket))
   , strand(ws.get_executor())
   , sd(sd_)
   { }

   void run()
   {
      auto handler = [p = shared_from_this()](auto ec)
      { p->on_accept(ec); };

      ws.async_accept(
         boost::asio::bind_executor(strand, handler));
   }

   void on_accept(boost::system::error_code ec)
   {
      if (ec)
         return fail(ec, "accept");

      do_read();
   }

   void do_read()
   {
      auto handler = [p = shared_from_this()](auto ec, auto n)
      { p->on_read(ec, n); };

      ws.async_read( buffer
                   , boost::asio::bind_executor(
                        strand,
                        handler));
   }

   void on_read( boost::system::error_code ec
               , std::size_t bytes_transferred)
   {
      boost::ignore_unused(bytes_transferred);

      // This indicates that the session was closed
      if (ec == websocket::error::closed)
         return;

      if (ec)
         fail(ec, "read");

      // Echo the message
      ws.text(ws.got_text());

      json resp;
      std::stringstream ss;
      ss << boost::beast::buffers(buffer.data());
      try {
         json j;
         ss >> j;
         std::cout << j << std::endl;
         auto cmd = j["cmd"].get<std::string>();
         if (cmd == "login") {
            auto name = j["name"].dump();
            auto tel = j["tel"].dump();
            std::cout << "Login:\n\n"
                      << "   Name: " << name << "\n"
                      << "   Tel.: " << tel << "\n"
                      << std::endl;
            json login_ack;
            resp["cmd"] = "login_ack";
            resp["result"] = "ok";

            // Handle list of contacts later.
            resp["user_idx"] = sd->add_user(tel, {});
         } else if (cmd == "msg") {
         } else {
            std::cerr << "Server: Unknown command "
                       << cmd << std::endl;
         }
      } catch (...) {
         std::cerr << "Server: Invalid json." << std::endl;
      }

      auto handler = [p = shared_from_this()](auto ec, auto n)
      { p->on_write(ec, n); };

      ws.async_write( boost::asio::buffer(resp.dump())
                    , boost::asio::bind_executor(
                         strand,
                         handler));
   }

   void on_write( boost::system::error_code ec
                , std::size_t bytes_transferred)
   {
      boost::ignore_unused(bytes_transferred);

      if (ec)
         return fail(ec, "write");

      // Clear the buffer
      buffer.consume(buffer.size());

      do_read();
   }
};

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener> {
private:
   tcp::acceptor acceptor;
   tcp::socket socket;
   std::shared_ptr<server_data> sd;

public:
   listener( boost::asio::io_context& ioc
           , tcp::endpoint endpoint)
   : acceptor(ioc)
   , socket(ioc)
   , sd(std::make_shared<server_data>())
   {
      boost::system::error_code ec;

      // Open the acceptor
      acceptor.open(endpoint.protocol(), ec);
      if (ec) {
         fail(ec, "open");
         return;
      }

      // Allow address reuse
      acceptor.set_option(boost::asio::socket_base::reuse_address(true));
      if (ec) {
         fail(ec, "set_option");
         return;
      }

      // Bind to the server address
      acceptor.bind(endpoint, ec);
      if (ec) {
         fail(ec, "bind");
         return;
      }

      // Start listening for connections
      acceptor.listen(
            boost::asio::socket_base::max_listen_connections, ec);
      if (ec) {
         fail(ec, "listen");
         return;
      }
   }

   // Start accepting incoming connections
   void run()
   {
      if (!acceptor.is_open())
         return;

      do_accept();
   }

   void do_accept()
   {
      auto handler = [p = shared_from_this()](auto ec)
      { p->on_accept(ec); };

      acceptor.async_accept(socket, handler);
   }

   void on_accept(boost::system::error_code ec)
   {
      if (ec) {
         fail(ec, "accept");
      } else {
         std::make_shared<session>( std::move(socket)
                                  , sd)->run();
      }

      // Accept another connection
      do_accept();
   }
};

int main(int argc, char* argv[])
{
   auto const address = boost::asio::ip::make_address("127.0.0.1");
   auto const port = static_cast<unsigned short>(8080);
   auto const threads = 1;

   boost::asio::io_context ioc{threads};

   std::make_shared<listener>(ioc, tcp::endpoint {address, port})->run();

   ioc.run();
   return EXIT_SUCCESS;
}

