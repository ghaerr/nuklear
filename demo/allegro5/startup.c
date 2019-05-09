int real_main(int ac, char **av);	/* ALLEGRO renamed main entry point*/
int main(int ac, char **av)
{
	/* required to force allegro to run OSX API calls on the main thread*/
	extern int al_run_main(int ac, char **av, int (*main_ptr)());
	return al_run_main(ac, av, real_main);
}


