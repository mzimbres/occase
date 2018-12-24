#include "fipe.hpp"

#include <deque>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace rt
{

enum table_field
{ id, tipo, id_modelo_ano, fipe_codigo, id_marca, marca
, id_modelo, modelo, ano, name, combustivel, preco
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

auto calc_indent(table_field field)
{
   if (field == table_field::tipo)        return 0;
   if (field == table_field::marca)       return 1;
   if (field == table_field::modelo)      return 2;
   if (field == table_field::ano)         return 3;
   if (field == table_field::combustivel) return 4;
   if (field == table_field::preco)       return 5;
   return 0;
}

auto next_field(table_field field)
{
   if (field == table_field::tipo)        return table_field::marca;
   if (field == table_field::marca)       return table_field::modelo;
   if (field == table_field::modelo)      return table_field::ano;
   if (field == table_field::ano)         return table_field::combustivel;
   if (field == table_field::combustivel) return table_field::preco;
   return table_field::fipe_codigo;
}

template <class Iter>
void print_partitions( Iter begin, Iter end, table_field field
                     , int indent_size)
{
   std::deque<std::deque<range>> st;
   st.push_back({{begin, end, table_field::tipo}});

   while (!std::empty(st)) {
      auto r = st.back().back();
      st.back().pop_back();
      if (std::empty(st.back()))
         st.pop_back();

      auto const n = calc_indent(r.field);
      std::string indentation(n * indent_size, ' ');
      std::cout << indentation << r.begin->at(r.field) << std::endl;

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
}

void fipe_dump(fipe_op const& op)
{
   std::ifstream ifs(op.file);

   std::vector<std::vector<std::string>> table;
   std::string line;
   while (std::getline(ifs, line)) {
      std::string item;
      std::istringstream iss(line);
      std::vector<std::string> fields;
      while (std::getline(iss, item, ';')) {
         item.erase( std::remove(std::begin(item), std::end(item), '"')
                   , std::end(item));
         fields.push_back(item);
      }

      // TODO: Include the proper header for this.
      //assert(std::size(fields) == 12);

      if (fields[table_field::tipo] == op.tipo)
         table.push_back(std::move(fields));
   }

   std::cout << "Table size: " << std::size(table) << std::endl;

   print_partitions( std::begin(table), std::end(table)
                   , table_field::marca, 3);
}

}

