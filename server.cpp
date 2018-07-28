#include <iostream>

#include <vector>
#include <unordered_map>

using id_type = long long;

struct user {
  // We will add new friends to a user much often than remove them,
  // therefore I will store them in a vector where insertion in the
  // end is O(1) and traversal is best. Removing a friend will const
  // O(n). Another option would be std::set but I want to avoid linked
  // data structures for now.
  std::vector<id_type> friends;
};

struct group {
  long long owner;
  std::vector<id_type> members;
};

struct data {
  std::unordered_map<id_type, user> users;
  std::vector<group> groups;

  void add_user(id_type id, user u)
  {
    // Consider insert_or_assign and also std::move.
    auto new_user = users.insert({id, u});
    if (!new_user.second) {
      // The user already exists. This case can be triggered by two
      // conditions: (1) The user inserted or removed a contact from
      // his phone and is sending us his new contacts. (2) The user
      // lost his phone or simply changed his number and the number
      // was assigned to a new person.
      // REVIEW: For now we will simply override previous values and
      // decide later how to handle (2) properly.

      // REVIEW: Is it correct to assume that if the insertion failed
      // than the user object remained unmoved? 
      new_user.first->second.friends = std::move(u.friends);
      return;
    }

    // Insertion took place and now we have to test which of its
    // friends are already and add them as user friends.
    for (auto const& o : u.friends) {
      auto existing_user = users.find(o);
      if (existing_user == std::end(users))
        continue;

      // A contact was found in our database, let us add him as the
      // user friend.
      new_user.first->second.friends.push_back(o);

      // REVIEW: We also have to inform the existing user that one of
      // his contacts entered in the game. This will be made only upon
      // request, for now, we will only add the new user in the
      // existing user's friends.
      existing_user->second.friends.push_back(new_user.first->first);
    }
  }
};

int main()
{
  std::cout << "sellit" << std::endl;
}

