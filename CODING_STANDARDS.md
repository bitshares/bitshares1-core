BitShares Toolkit Coding Standards
--------------------------

BitShares Toolkit is a set C++ libraries that follow conventions similar to boost or the standard template library.

  1) All text should be lowercase with the exception of MACROS and pre-processor defines.

  2) All private member variables should begin with '_'

  3) The following example is a template of how a high level class should be implemented.  This is done to
     minimize header bloat and keep the private implementation details out of the header.

      namespace a { namespace b { 

         namespace deltail { some_class_impl; }

         class some_class : public base_class
         {
            public:
                some_clase();
                ~some_class();

            private:
                std::unique_ptr<detail::some_class_impl> my;
         };

      } } // a::b

   4) All virtual methods should inlcude the 'virtual' keyword even in derived classes.

   5) Braces should use the following convention:

      if( condition )
      {

      } else if( other_condition ) {
      
      }

      while( condition )
      {
      }

      try {
        ....
      } 
      catch ( const fc::exception& e )
      {

      }

   
