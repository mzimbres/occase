#include "fipe.hpp"

#include <deque>
#include <vector>
#include <cassert>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace rt
{

enum table_field
{ id, tipo, id_modelo_ano, fipe_codigo, id_marca, marca
, id_modelo, modelo1, ano, name, combustivel, preco, modelo2
};

struct line_comp {
   table_field field;
   auto operator()( std::vector<std::string> const& v1
                  , std::vector<std::string> const& v2) const
   { return v1.at(field) < v2.at(field); };
};

struct line_comp_pred {
   std::string s;
   table_field field;
   auto operator()(std::vector<std::string> const& v) const
   { return s == v.at(field); }
};

struct range {
   using iter_type = std::vector<std::vector<std::string>>::iterator;
   iter_type begin;
   iter_type end;
   table_field field;
};

// The indentation sze returned by this function depends on the order
// configured in next_field. Both function have to be changed
// together.
auto calc_indent(table_field field)
{
   if (field == table_field::tipo)        return 0;
   if (field == table_field::marca)       return 1;
   if (field == table_field::modelo1)     return 2;
   if (field == table_field::modelo2)     return 4;
   if (field == table_field::ano)         return 3;
   if (field == table_field::combustivel) return 5;
   if (field == table_field::preco)       return 6;
   return 0;
}

auto next_field(table_field field)
{
   if (field == table_field::tipo)         return table_field::marca;
   if (field == table_field::marca)        return table_field::modelo1;
   if (field == table_field::modelo1)      return table_field::ano;
   if (field == table_field::modelo2)      return table_field::combustivel;
   if (field == table_field::ano)          return table_field::modelo2;
   if (field == table_field::combustivel)  return table_field::preco;
   return table_field::fipe_codigo;
}

auto get_indent_str(table_field f, int i)
{
   auto const n = calc_indent(f);

   if (i == -1) {
      auto str = std::to_string(n);
      str += " ";
      return str;
   }

   return std::string(n * i, ' ');
}

template <class Iter>
std::string
print_partitions(Iter begin, Iter end, int indent_size, char c)
{
   std::deque<std::deque<range>> st;
   st.push_back({{begin, end, table_field::tipo}});

   std::ostringstream oss;
   while (!std::empty(st)) {
      auto r = st.back().back();
      st.back().pop_back();
      if (std::empty(st.back()))
         st.pop_back();

      assert(!std::empty(r.begin->at(r.field)));
      oss << get_indent_str(r.field, indent_size)
          << r.begin->at(r.field)
          << c;

      auto const next = next_field(r.field);
      std::sort(r.begin, r.end, line_comp {next});

      if (next == table_field::fipe_codigo)
         continue;

      std::deque<range> foo;
      Iter iter = r.begin;
      while (iter != r.end) {
         auto point = std::partition_point( iter, r.end
                                          , line_comp_pred
                                            {iter->at(next), next});
         foo.push_front({iter, point, next});
         iter = point;
      }

      st.push_back(foo);
   }

   return oss.str();
}

std::string split_model(std::string& model)
{
   // The first naive strategy is to split on the first space found.
   auto const i = model.find(' ');
   if (i == std::string::npos) {
      // The string is composed of only one field. Not much we can do.
      return "Todos";
   }

   if (i == 0) {
      throw std::runtime_error("Invalid field.");
   }


   auto const j = model.find_first_not_of(' ', i + 1);
   auto const model2 = model.substr(j);
   if (std::empty(model2)) {
      // The space character was the last on the line. Not much we can
      // do again.
      return "Todos";
   }

   model.erase(i);
   //std::cout << std::setw(20) << std::left << model << " ===> " << model2 << std::endl;

   assert(!std::empty(model2));
   return model2;
}

std::string
fipe_dump( std::string const& str, int indentation
         , std::string const& tipo, char c)
{
   std::istringstream iss(str);

   std::vector<std::vector<std::string>> table;
   std::string line;
   while (std::getline(iss, line)) {
      line.erase( std::remove(std::begin(line), std::end(line), '"')
                , std::end(line));
      std::string item;
      std::istringstream iss(line);
      std::vector<std::string> fields;
      while (std::getline(iss, item, ';')) {
         fields.push_back(item);
      }

      // TODO: Include the proper header for this.
      assert(std::size(fields) == 12);

      auto const modelo2 = split_model(fields[table_field::modelo1]);
      fields.push_back(modelo2);

      if (fields[table_field::tipo] == tipo)
         table.push_back(std::move(fields));
   }

   if (std::empty(table))
      return {};

   return print_partitions( std::begin(table), std::end(table)
                          , indentation, c);
}

}

