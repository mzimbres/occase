#include <stack>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <iterator>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include "menu.hpp"

using namespace rt;

struct menu_op {
   int menu;
   int indentation;
   int sim_length;
   bool hash = false;
   std::string file {"menus/cidades"};
};

void bar(menu_op op)
{
   std::string menu_str =
   "Brasil\n"
   "   Sao Paulo\n"
   "      Atibaia\n"
   "         Vila Santista\n"
   "         Jardim Siriema\n"
   "      Braganca\n"
   "      Piracaia\n"
   "      Sao Paulo\n"
   "         Mooca\n"
   "         Bixiga\n"
   "         Vila Leopoldina\n"
   "   Rio de Janeiro\n"
   "      Teresópolis\n"
   "      Niteroi\n"
   "   Amazonas\n"
   "      Manaus\n"
   "   Paraiba\n"
   "   Bahia\n"
   ;

   std::cout << menu_str << std::endl;
   menu m {menu_str};

   auto const codes = m.get_codes();
   for (auto const& c : codes)
      std::cout << c << std::endl;
   std::cout << std::endl;

   m.dump();

   for (auto const& o: m.get_codes())
      std::cout << o << "\n";
   std::cout << std::endl;

}

void from_file(menu_op op)
{
   std::ifstream ifs(op.file);

   using iter_type = std::istreambuf_iterator<char>;
   std::string menu_str {iter_type {ifs}, {}};

   menu m {menu_str};

   for (auto const& o: m.get_codes())
      std::cout << o << "\n";
   std::cout << std::endl;

   m.dump();
}

enum fipe_fields
{ id, tipo, id_modelo_ano, fipe_codigo, id_marca, marca
, id_modelo, modelo, ano, name, combustivel, preco
};

struct comp_helper {
   std::string s;
   fipe_fields f;
   auto operator()(std::vector<std::string> const& v) const
   {
      return s == v.at(f);
   }
};

struct line_comp {
   fipe_fields f;
   auto operator()( std::vector<std::string> const& v1
                  , std::vector<std::string> const& v2) const
   { return v1.at(f) < v2.at(f); };
};

struct helper {
   using iter_type = std::vector<std::vector<std::string>>::iterator;
   iter_type begin;
   iter_type end;
   fipe_fields f;
};

auto calc_indent(fipe_fields f)
{
   if (f == fipe_fields::tipo)   return 0;
   if (f == fipe_fields::marca)  return 1;
   if (f == fipe_fields::modelo) return 2;
   if (f == fipe_fields::ano)    return 3;
   if (f == fipe_fields::preco)  return 4;
   return 0;
}

auto next_field(fipe_fields f)
{
   if (f == fipe_fields::tipo)   return fipe_fields::marca;
   if (f == fipe_fields::marca)  return fipe_fields::modelo;
   if (f == fipe_fields::modelo) return fipe_fields::ano;
   if (f == fipe_fields::ano) return fipe_fields::preco;
   return fipe_fields::preco;
}

template <class Iter>
void print_partitions(Iter begin, Iter end, fipe_fields f)
{
   std::deque<std::deque<helper>> st;
   st.push_back({{begin, end, fipe_fields::tipo}});

   std::cout << "Marcas" << std::endl;

   while (!std::empty(st)) {
      auto h = st.back().back();
      st.back().pop_back();
      if (std::empty(st.back()))
         st.pop_back();

      auto const next = next_field(h.f);
      std::sort(h.begin, h.end, line_comp {next});

      auto const n = calc_indent(next);
      std::string indentation(n * 3, ' ');
      std::cout << indentation << h.begin->at(next) << std::endl;

      if (next == fipe_fields::preco)
         continue;

      std::deque<helper> foo;
      Iter iter = h.begin;
      while (iter != h.end) {
         auto point = std::partition_point( iter, h.end
                                          , comp_helper
                                            {iter->at(next), next});
         foo.push_front({iter, point, next});
         iter = point;
      }

      st.push_back(foo);
   }
}

void fipe_csv(menu_op op)
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

      assert(std::size(fields) == 12);

      if (fields[fipe_fields::tipo] == "1")
         table.push_back(std::move(fields));
   }

   std::cout << "Table size: " << std::size(table) << std::endl;

   //for (auto const& o : table)
   //   std::cout << o.at(fipe_fields::marca) << " ===> " << o.at(fipe_fields::modelo) << std::endl;

   print_partitions( std::begin(table), std::end(table)
                   , fipe_fields::marca);
}

void foo(json menu, menu_op op)
{
   if (op.hash) {
      // TODO: Output codes with indentation.
      auto const hashes = get_hashes(menu);
      for (auto const& o : hashes)
         std::cout << o << "\n";
      return;
   }

   std::cout << menu.dump(op.indentation) << std::endl;
}

void op0(menu_op op)
{
   auto const menu = gen_location_menu();
   foo(menu, op);
}

void op1(menu_op op)
{
   auto const menu = gen_sim_menu(op.sim_length);
   std::cout << menu << std::endl;
   //foo(menu, op);
}

json gen_location_menu1()
{
   std::vector<json> j3 =
   { {{"hash", ""}, {"name", "Atibaia"},   {"sub", {}}}
   , {{"hash", ""}, {"name", "Sao Paulo"}, {"sub", {}}}
   };

   json j;
   j["name"] = "SP";
   j["sub_desc"] = "Cidades";
   j["sub"] = j3;

   return j;
}

json gen_location_menu3()
{
   std::vector<json> j1 =
   { {{"hash", ""}, {"sub", {}}, {"name", "Centro"}}};

   std::vector<json> j2 =
   { {{"hash", ""}, {"sub", {}}, {"name", "Vila Leopoldina"}, } };

   std::vector<json> j3 =
   { {{"name", "Atibaia"},  {"sub_desc", "Bairros"}, {"sub", j1}}
   , {{"name", "Sao Paulo"},{"sub_desc", "Bairros"}, {"sub", j2}}
   };

   json j;
   j["name"] = "SP";
   j["sub_desc"] = "Cidades";
   j["sub"] = j3;

   return j;
}

json gen_location_menu0()
{
   json j;
   j["name"] = "SP";
   j["sub_desc"] = "Cidades";
   j["sub"] = {};
   j["hash"] = "";

   return j;
}

namespace po = boost::program_options;

int main(int argc, char* argv[])
{
   menu_op op;
   po::options_description desc("Options");
   desc.add_options()
      ("help,h", "produce help message")
      ("menu,m"
      , po::value<int>(&op.menu)->default_value(0)
      , "Choose the menu. Available options:\n"
        " 0: Atibaia - Sao Paulo.\n"
        " 1: Simulated.\n"
        " 2: New menu.\n"
        " 3: From file.\n"
        " 4: CSV Fipe file.\n"
      )
      ("indentation,i"
      , po::value<int>(&op.indentation)->default_value(-1)
      , "Indentation of the menu output.")
      ("sim-length,l"
      , po::value<int>(&op.sim_length)->default_value(2)
      , "Length of simulated children.")
      ("hash,a", "Output channel codes only.")
      ("fipe-csv-file,r"
      , po::value<std::string>(&op.file)
      , "Fipe CSV file containing all vehicles.")
   ;

   po::variables_map vm;        
   po::store(po::parse_command_line(argc, argv, desc), vm);
   po::notify(vm);    

   if (vm.count("help")) {
      std::cout << desc << "\n";
      return 0;
   }

   if (vm.count("hash"))
      op.hash = true;

   switch (op.menu) {
      case 1: op1(op); break;
      case 2: bar(op); break;
      case 3: from_file(op); break;
      case 4: fipe_csv(op); break;
      default:
         op0(op);
   }

   return 0;
}

